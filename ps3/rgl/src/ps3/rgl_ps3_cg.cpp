/*  RetroArch - A frontend for libretro.
 *  RGL - An OpenGL subset wrapper library.
 *  Copyright (C) 2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2012 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../rgl_cg.h"

static CGbool rglpSupportsVertexProgram( CGprofile p )
{
   if ( p == CG_PROFILE_SCE_VP_TYPEB )
      return CG_TRUE;
   if ( p == CG_PROFILE_SCE_VP_TYPEC )
      return CG_TRUE;
   if ( p == CG_PROFILE_SCE_VP_RSX )
      return CG_TRUE;
   return CG_FALSE;
}

static CGbool rglpSupportsFragmentProgram( CGprofile p )
{
   if ( p == CG_PROFILE_SCE_FP_TYPEB )
      return CG_TRUE;
   if ( CG_PROFILE_SCE_FP_RSX == p )
      return CG_TRUE;
   return CG_FALSE;
}

static CGprofile rglpGetLatestProfile( CGGLenum profile_type )
{
   switch ( profile_type )
   {
      case CG_GL_VERTEX:
         return CG_PROFILE_SCE_VP_RSX;
      case CG_GL_FRAGMENT:
         return CG_PROFILE_SCE_FP_RSX;
      default:
         break;
   }
   return CG_PROFILE_UNKNOWN;
}

static int rglGcmGenerateProgram (void *data, int profileIndex, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader,
      const CgParameterEntry *parameterEntries, const char *stringTable, const float *defaultValues )
{
   _CGprogram *program = (_CGprogram*)data;
   CGprofile profile = ( CGprofile )programHeader->profile;

   int need_swapping = 0;

   //hack to counter removal of TypeC during beta
   if ( profile == ( CGprofile )7005 )
      profile = CG_PROFILE_SCE_VP_RSX;
   if ( profile == ( CGprofile )7006 )
      profile = CG_PROFILE_SCE_FP_RSX;

   // if can't match a known profile, the data may be in wrong endianness
   if (( profile != CG_PROFILE_SCE_FP_TYPEB ) && ( profile != CG_PROFILE_SCE_VP_TYPEB ) &&
         ( profile != CG_PROFILE_SCE_FP_RSX ) && ( profile != CG_PROFILE_SCE_VP_RSX ) )
   {
      need_swapping = 1;
   }

   // check that this program block is of the right revision
   // i.e. that the cgBinary.h header hasn't changed since it was
   // compiled.

   // validate the profile
   int invalidProfile = 0;
   switch ( ENDIAN_32( profile, need_swapping ) )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
         if ( profileIndex != VERTEX_PROFILE_INDEX ) invalidProfile = 1;
         break;
      case CG_PROFILE_SCE_FP_TYPEB:
         if ( profileIndex != FRAGMENT_PROFILE_INDEX ) invalidProfile = 1;
         break;
      case CG_PROFILE_SCE_VP_RSX:
         if ( profileIndex != VERTEX_PROFILE_INDEX ) invalidProfile = 1;
         break;
      case CG_PROFILE_SCE_FP_RSX:
         if ( profileIndex != FRAGMENT_PROFILE_INDEX ) invalidProfile = 1;
         break;
      default:
         invalidProfile = 1;
         break;
   }
   if ( invalidProfile )
   {
      rglCgRaiseError( CG_UNKNOWN_PROFILE_ERROR );
      return 0;
   }

   memcpy( &program->header, programHeader, sizeof( program->header ) );

   program->ucode = ucode;
   program->loadProgramId = GMM_ERROR;
   program->loadProgramOffset = 0;
   program->inLocalMemory = true; 


   size_t parameterSize = parameterHeader->entryCount * sizeof( CgRuntimeParameter );
   void *memoryBlock;
   if ( parameterSize )
      memoryBlock = memalign( 16, parameterSize );
   else
      memoryBlock = NULL;

   program->rtParametersCount = parameterHeader->entryCount;
   program->runtimeParameters = ( CgRuntimeParameter* )memoryBlock;

   if ( parameterEntries == NULL ) // the param entry can be supplied if not right after parameterHeader in memory, it happens when there's a program copy
      parameterEntries = ( CgParameterEntry* )( parameterHeader + 1 );

   program->parametersEntries = parameterEntries;
   program->parameterResources = ( char* )( program->parametersEntries + program->rtParametersCount );
   program->resources = ( unsigned short* )(( char* )program->parametersEntries + ( parameterHeader->resourceTableOffset - sizeof( CgParameterTableHeader ) ) );
   program->defaultValuesIndexCount = parameterHeader->defaultValueIndexCount;
   program->defaultValuesIndices = ( CgParameterDefaultValue* )(( char* )program->parametersEntries + ( parameterHeader->defaultValueIndexTableOffset - sizeof( CgParameterTableHeader ) ) );
   program->semanticCount = parameterHeader->semanticIndexCount;
   program->semanticIndices = ( CgParameterSemantic* )( program->defaultValuesIndices + program->defaultValuesIndexCount );

   program->defaultValues = NULL;

   memset( program->runtimeParameters, 0, parameterHeader->entryCount*sizeof( CgRuntimeParameter ) );

   //string table
   program->stringTable = stringTable;
   //default values
   program->defaultValues = defaultValues;

   rglCreatePushBuffer( program );
   int count = program->defaultValuesIndexCount;

   if ( profileIndex != FRAGMENT_PROFILE_INDEX )
   {
      /* modifies the push buffer */

      for (int i = 0; i < count; i++)
      {
         int index = ( int )program->defaultValuesIndices[i].entryIndex;
         CgRuntimeParameter *rtParameter = program->runtimeParameters + index;

         int arrayCount = 1;
         const CgParameterEntry *parameterEntry = rtParameter->parameterEntry;
         bool isArray = false;
         if ( parameterEntry->flags & CGP_ARRAY )
         {
            isArray = true;
            const CgParameterArray *parameterArray = rglGetParameterArray( program, parameterEntry );
            arrayCount = rglGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount );
            parameterEntry++;
            rtParameter++;
         }

         if ( rtParameter->pushBufferPointer ) //unreferenced might have default values
         {
            const CgParameterResource *parameterResource = rglGetParameterResource( program, parameterEntry );
            const float *itemDefaultValues = program->defaultValues + program->defaultValuesIndices[i].defaultValueIndex;
            int registerStride = isMatrix(( CGtype )parameterResource->type ) ? rglGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
            if ( parameterEntry->flags & CGP_CONTIGUOUS )
               __builtin_memcpy( rtParameter->pushBufferPointer, itemDefaultValues, arrayCount * registerStride *4*sizeof( float ) );
            else
            {
               unsigned int *pushBufferPointer = (( unsigned int * )rtParameter->pushBufferPointer );
               for ( int j = 0;j < arrayCount;j++ )
               {
                  unsigned int *pushBufferAddress = isArray ? ( *( unsigned int** )pushBufferPointer ) : pushBufferPointer;
                  __builtin_memcpy( pushBufferAddress, itemDefaultValues, registerStride*4*sizeof( float ) );
                  pushBufferPointer += isArray ? 1 : 3 + registerStride * 4;
                  itemDefaultValues += 4 * registerStride;
               }
            }
         }
      }
   }

   // not loaded yet
   program->loadProgramId = GMM_ERROR;
   program->loadProgramOffset = 0;
   if ( profileIndex == FRAGMENT_PROFILE_INDEX )
   {
      // always load fragment shaders.
      // uploads the given fp shader to gpu memory. Allocates if needed.
      // This also builds the shared constants push buffer if needed, since it depends on the load address
      unsigned int ucodeSize = program->header.instructionCount * 16;

      if ( program->loadProgramId == GMM_ERROR )
      {
         program->loadProgramId = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo,
               0, ucodeSize);
         program->loadProgramOffset = 0;
      }

      rglGcmSend( program->loadProgramId, program->loadProgramOffset, 0, ( char* )program->ucode, ucodeSize );
   }

   program->programGroup = NULL;
   program->programIndexInGroup = -1;

   return 1;
}

CGprogram rglpCgUpdateProgramAtIndex( CGprogramGroup group, int index, int refcount )
{
   if (index < ( int )group->programCount)
   {
      //index can be < 0 , in that case refcount update on the group only, used when destroying a copied program
      if (index >= 0)
      {
         //if it has already been referenced duplicate instead of returning the same index, until the API offer a native support for
         //group of programs ( //fixed bug 13007 )
         if (refcount == 1 && group->programs[index].refCount == 1)
         {
            //it will handle the refcounting
            CGprogram res = cgCopyProgram( group->programs[index].program );
            return res;
         }
         group->programs[index].refCount += refcount;
      }

      group->refCount += refcount;
      if (refcount < 0)
      {
         if (group->refCount == 0 && !group->userCreated)
            rglCgDestroyProgramGroup( group );
         return NULL;
      }
      else
         return group->programs[index].program;
   }
   else
      return NULL;
}

static CGprogramGroup rglCgCreateProgramGroupFromFile( CGcontext ctx, const char *group_file )
{
   // check that file exists
   FILE* fp = fopen( group_file, "rb" );

   if ( NULL == fp )
   {
      rglCgRaiseError( CG_FILE_READ_ERROR );
      return ( CGprogramGroup )NULL;
   }

   // find the file length
   size_t file_size = 0;
   fseek( fp, 0, SEEK_END );
   file_size = ftell( fp );
   rewind( fp );

   // alloc memory for new binary program and read the data
   char* ptr = ( char* )malloc( file_size + 1 );
   if ( NULL == ptr )
   {
      rglCgRaiseError( CG_MEMORY_ALLOC_ERROR );
      return ( CGprogramGroup )NULL;
   }

   // read the entire file into memory then close the file
   // TODO ********* just loading the file is a bit lame really. We can do better.
   fread( ptr, file_size, 1, fp );
   fclose( fp );

   CGprogramGroup group = rglCgCreateProgramGroup( ctx, group_file, ptr, file_size );
   if ( !group )
      free( ptr );

   return group;
}

CGprogramGroup rglCgCreateProgramGroup( CGcontext ctx,  const char *name, void *ptr, int size )
{
   _CGprogramGroup *group = NULL;
   CGELFBinary elfBinary;
   elfBinary.elfFile = NULL;

   while ( 1 )
   {
      bool res = cgOpenElf( ptr, size, &elfBinary );
      if ( !res )
         break;

      size_t elfConstTableSize = ( size_t )elfBinary.consttabSize;
      size_t elfStringTableSize = ( size_t )elfBinary.strtabSize;

      //first pass to get the size of each item ( could be faster if the embedded constants index table size was in the header
      int programCount = elfBinary.shadertabSize / sizeof( CgProgramHeader );
      int i;

      //structure of the memory buffer storing the group
      size_t nvProgramNamesOffset = rglPad( sizeof( _CGprogramGroup ), sizeof( _CGnamedProgram ) ); //program name offset
      size_t nvDefaultValuesTableOffset = rglPad( nvProgramNamesOffset + programCount * sizeof( _CGnamedProgram ), 16 );//shared default value table

      size_t nvStringTableOffset = nvDefaultValuesTableOffset + elfConstTableSize; //shared string table
      size_t structureSize = nvStringTableOffset + elfStringTableSize;//total structure size

      //create the program group
      group = ( CGprogramGroup )malloc( structureSize );
      if ( !group ) //out of memory
         break;

      //fill the group structure
      group->ctx = ctx;
      group->next = NULL;
      group->programCount = ( unsigned int )programCount;
      group->constantTable = ( unsigned int * )(( char* )group + nvDefaultValuesTableOffset );
      group->stringTable = ( unsigned int * )(( char* )group + nvStringTableOffset );
      group->programs = ( _CGnamedProgram * )(( char* )group + nvProgramNamesOffset );
      group->userCreated = true;
      group->refCount = 0;
      group->filedata = ( char* )ptr;
      if ( name )
      {
         int len = strlen( name );
         group->name = ( char* )malloc( len + 1 );
         if ( !group->name )//out of memory
            break;
         strcpy( group->name, name );
      }
      else
         group->name = NULL;

      //copy the default values
      if ( elfConstTableSize )
         memcpy(( char* )group + nvDefaultValuesTableOffset, elfBinary.consttab, elfConstTableSize );
      //copy the string table
      if ( elfStringTableSize )
         memcpy(( char* )group + nvStringTableOffset, elfBinary.strtab, elfStringTableSize );

      //add the group to the context:
      _CGcontext *context = _cgGetContextPtr(ctx);
      if ( !context->groupList )
         context->groupList = group;
      else
      {
         _CGprogramGroup *current = context->groupList;
         while ( current->next )
            current = current->next;
         current->next = group;
      }

      //create all the shaders contained in the package and add them to the group
      for ( i = 0;i < ( int )group->programCount;i++ )
      {
         CgProgramHeader *cgShader = ( CgProgramHeader* )elfBinary.shadertab + i;

         //hack to counter removal of TypeC during beta
         if ( cgShader->profile == ( CGprofile )7005 )
            cgShader->profile = CG_PROFILE_SCE_VP_RSX;
         if ( cgShader->profile == ( CGprofile )7006 )
            cgShader->profile = CG_PROFILE_SCE_FP_RSX;

         CGELFProgram elfProgram;
         bool res = cgGetElfProgramByIndex( &elfBinary, i, &elfProgram );
         if ( !res )
            return false;

         //I reference the buffer passed as parameter here, so it will have to stay around
         CgProgramHeader *programHeader = cgShader;
         char *ucode = ( char * )elfProgram.texttab;
         CgParameterTableHeader *parameterHeader  = ( CgParameterTableHeader * )elfProgram.paramtab;

         const char *programName = getSymbolByIndexInPlace( elfBinary.symtab, elfBinary.symbolSize, elfBinary.symbolCount, elfBinary.symbolstrtab, i + 1 );
         group->programs[i].name = programName;
         group->programs[i].program = rglCgCreateProgram( ctx, ( CGprofile )cgShader->profile, programHeader, ucode, parameterHeader, ( const char* )group->stringTable, ( const float* )group->constantTable );
         _CGprogram *cgProgram = _cgGetProgPtr( group->programs[i].program );
         cgProgram->programGroup = group;
         cgProgram->programIndexInGroup = i;
         group->programs[i].refCount = 0;
      }
      break;
   }

   return group;
}

void rglCgDestroyProgramGroup( CGprogramGroup group )
{
   _CGprogramGroup *_group = ( _CGprogramGroup * )group;
   for ( int i = 0;i < ( int )_group->programCount;i++ )
   {
      //unlink the program
      _CGprogram *cgProgram = _cgGetProgPtr( group->programs[i].program );
      cgProgram->programGroup = NULL;
      cgDestroyProgram( _group->programs[i].program );
   }
   free( _group->filedata );
   if ( _group->name )
      free( _group->name );

   //remove the group from the group list
   _CGcontext *context = _cgGetContextPtr( group->ctx );
   _CGprogramGroup *current = context->groupList;
   _CGprogramGroup *previous = NULL;
   while ( current && current != group )
   {
      previous = current;
      current = current->next;
   }
   if ( current )
   {
      if ( !previous )
         context->groupList = current->next;
      else
         previous->next = current->next;
   }
   free( _group );
}

const char *rglCgGetProgramGroupName (CGprogramGroup group)
{
   _CGprogramGroup *_group = ( _CGprogramGroup * )group;
   return _group->name;
}

int rglCgGetProgramIndex (CGprogramGroup group, const char *name)
{
   int i;
   for ( i = 0;i < ( int )group->programCount;i++ )
   {
      if ( !strcmp( name, group->programs[i].name ) )
         return i;
   }
   return -1;
}

void rglpProgramErase (void *data)
{
   _CGprogram* platformProgram = (_CGprogram*)data;
   _CGprogram* program = (_CGprogram*)platformProgram;

   if ( program->loadProgramId != GMM_ERROR )
   {
      gmmFree( program->loadProgramId );
      program->loadProgramId = GMM_ERROR;
      program->loadProgramOffset = 0;
   }

   //free the runtime parameters
   if ( program->runtimeParameters )
   {
      //need to erase all the program parameter "names"
      int i;
      int count = ( int )program->rtParametersCount;

      for ( i = 0; i < count;i++ )
         rglEraseName( &_CurrentContext->cgParameterNameSpace, (unsigned int)program->runtimeParameters[i].id );

      free( program->runtimeParameters );
   }

   //free the push buffer block
   if ( program->memoryBlock )
      free( program->memoryBlock );

   //free the samplers lookup tables
   if ( program->samplerIndices )
   {
      free( program->samplerValuesLocation );
      free( program->samplerIndices );
      free( program->samplerUnits );
   }

   //free the "pointers" on the push buffer used for fast access
   if ( program->constantPushBufferPointers )
      free( program->constantPushBufferPointers );
}

//TODO: use a ref mechanism for the string table or duplicate it !
int rglpCopyProgram (void *src_data, void *dst_data)
{
   _CGprogram *source = (_CGprogram*)src_data;
   _CGprogram *destination = (_CGprogram*)dst_data;
   //extract the layout of the parameter buffers from the source
   CgParameterTableHeader parameterHeader;
   parameterHeader.entryCount = source->rtParametersCount;
   parameterHeader.resourceTableOffset = ( uintptr_t )(( char* )source->resources - ( char* )source->parametersEntries + sizeof( CgParameterTableHeader ) );
   parameterHeader.defaultValueIndexCount = source->defaultValuesIndexCount;
   parameterHeader.defaultValueIndexTableOffset = ( uintptr_t )(( char* )source->defaultValuesIndices - ( char* )source->parametersEntries + sizeof( CgParameterTableHeader ) );
   parameterHeader.semanticIndexCount = source->semanticCount;
   parameterHeader.semanticIndexTableOffset = ( uintptr_t )(( char* )source->defaultValuesIndices - ( char* )source->parametersEntries + sizeof( CgParameterTableHeader ) );

   int profileIndex;

   //allocate the copy of the program
   switch ( source->header.profile )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
      case CG_PROFILE_SCE_VP_RSX:
         profileIndex = VERTEX_PROFILE_INDEX;
         break;

      case CG_PROFILE_SCE_FP_TYPEB:
      case CG_PROFILE_SCE_FP_RSX:
         profileIndex = FRAGMENT_PROFILE_INDEX;
         break;
      default:
         return 0;
   }
   return rglGcmGenerateProgram( destination, profileIndex, &source->header, source->ucode, &parameterHeader, source->parametersEntries, source->stringTable, source->defaultValues );
}

int rglpGenerateVertexProgram (void *data, const CgProgramHeader *programHeader,
      const void *ucode, const CgParameterTableHeader *parameterHeader, const char *stringTable,
      const float *defaultValues )
{
   _CGprogram *program = (_CGprogram*)data;
   return rglGcmGenerateProgram( program, VERTEX_PROFILE_INDEX, programHeader,
         ucode, parameterHeader, NULL, stringTable, defaultValues );

}

int rglpGenerateFragmentProgram (void *data, const CgProgramHeader *programHeader, const void *ucode,
      const CgParameterTableHeader *parameterHeader, const char *stringTable, const float *defaultValues )
{
   _CGprogram *program = (_CGprogram*)data;
   return rglGcmGenerateProgram( program, FRAGMENT_PROFILE_INDEX, programHeader, ucode, parameterHeader, NULL, stringTable, defaultValues );

}

/*============================================================
  RTC CGC
  ============================================================ */

void cgRTCgcInit( void )
{
   _cgRTCgcCompileProgramHook = &compile_program_from_string;
   _cgRTCgcFreeCompiledProgramHook = &free_compiled_program;
}

void cgRTCgcFree( void )
{
   _cgRTCgcCompileProgramHook = 0;
   _cgRTCgcFreeCompiledProgramHook = 0;
}

/*============================================================
  CG COMMON
  ============================================================ */

void rglCgRaiseError( CGerror error )
{
   _CurrentContext->RGLcgLastError = error;

   //_RGL_REPORT_EXTRA( RGL_REPORT_CG_ERROR, "Cg error:%s", cgGetErrorString( error ) );

   if ( _CurrentContext->RGLcgErrorCallbackFunction )
      _CurrentContext->RGLcgErrorCallbackFunction();
}

unsigned int rglCountFloatsInCgType( CGtype type )
{
   int size = 0;
   switch ( type )
   {
      case CG_FLOAT:
      case CG_FLOAT1:
      case CG_FLOAT1x1:
         size = 1;
         break;
      case CG_FLOAT2:
      case CG_FLOAT2x1:
      case CG_FLOAT1x2:
         size = 2;
         break;
      case CG_FLOAT3:
      case CG_FLOAT3x1:
      case CG_FLOAT1x3:
         size = 3;
         break;
      case CG_FLOAT4:
      case CG_FLOAT4x1:
      case CG_FLOAT1x4:
      case CG_FLOAT2x2:
         size = 4;
         break;
      case CG_FLOAT2x3:
      case CG_FLOAT3x2:
         size = 6;
         break;
      case CG_FLOAT2x4:
      case CG_FLOAT4x2:
         size = 8;
         break;
      case CG_FLOAT3x3:
         size = 9;
         break;
      case CG_FLOAT3x4:
      case CG_FLOAT4x3:
         size = 12;
         break;
      case CG_FLOAT4x4:
         size = 16;
         break;
      case CG_SAMPLER1D:
      case CG_SAMPLER2D:
      case CG_SAMPLER3D:
      case CG_SAMPLERRECT:
      case CG_SAMPLERCUBE:
         size = 1;
         break;
      case CG_BOOL:
         size = 1;
         break;
      case CG_HALF:
      case CG_HALF1:
      case CG_HALF1x1:
         size = 1;
         break;
      case CG_HALF2:
      case CG_HALF2x1:
      case CG_HALF1x2:
         size = 2;
         break;
      case CG_HALF3:
      case CG_HALF3x1:
      case CG_HALF1x3:
         size = 3;
         break;
      case CG_HALF4:
      case CG_HALF4x1:
      case CG_HALF1x4:
      case CG_HALF2x2:
         size = 4;
         break;
      case CG_HALF2x3:
      case CG_HALF3x2:
         size = 6;
         break;
      case CG_HALF2x4:
      case CG_HALF4x2:
         size = 8;
         break;
      case CG_HALF3x3:
         size = 9;
         break;
      case CG_HALF3x4:
      case CG_HALF4x3:
         size = 12;
         break;
      case CG_HALF4x4:
         size = 16;
         break;
      case CG_INT:
      case CG_INT1:
      case CG_INT1x1:
         size = 1;
         break;
      case CG_INT2:
      case CG_INT2x1:
      case CG_INT1x2:
         size = 2;
         break;
      case CG_INT3:
      case CG_INT3x1:
      case CG_INT1x3:
         size = 3;
         break;
      case CG_INT4:
      case CG_INT4x1:
      case CG_INT1x4:
      case CG_INT2x2:
         size = 4;
         break;
      case CG_INT2x3:
      case CG_INT3x2:
         size = 6;
         break;
      case CG_INT2x4:
      case CG_INT4x2:
         size = 8;
         break;
      case CG_INT3x3:
         size = 9;
         break;
      case CG_INT3x4:
      case CG_INT4x3:
         size = 12;
         break;
      case CG_INT4x4:
         size = 16;
         break;
      case CG_BOOL1:
      case CG_BOOL1x1:
         size = 1;
         break;
      case CG_BOOL2:
      case CG_BOOL2x1:
      case CG_BOOL1x2:
         size = 2;
         break;
      case CG_BOOL3:
      case CG_BOOL3x1:
      case CG_BOOL1x3:
         size = 3;
         break;
      case CG_BOOL4:
      case CG_BOOL4x1:
      case CG_BOOL1x4:
      case CG_BOOL2x2:
         size = 4;
         break;
      case CG_BOOL2x3:
      case CG_BOOL3x2:
         size = 6;
         break;
      case CG_BOOL2x4:
      case CG_BOOL4x2:
         size = 8;
         break;
      case CG_BOOL3x3:
         size = 9;
         break;
      case CG_BOOL3x4:
      case CG_BOOL4x3:
         size = 12;
         break;
      case CG_BOOL4x4:
         size = 16;
         break;
      case CG_FIXED:
      case CG_FIXED1:
      case CG_FIXED1x1:
         size = 1;
         break;
      case CG_FIXED2:
      case CG_FIXED2x1:
      case CG_FIXED1x2:
         size = 2;
         break;
      case CG_FIXED3:
      case CG_FIXED3x1:
      case CG_FIXED1x3:
         size = 3;
         break;
      case CG_FIXED4:
      case CG_FIXED4x1:
      case CG_FIXED1x4:
      case CG_FIXED2x2:
         size = 4;
         break;
      case CG_FIXED2x3:
      case CG_FIXED3x2:
         size = 6;
         break;
      case CG_FIXED2x4:
      case CG_FIXED4x2:
         size = 8;
         break;
      case CG_FIXED3x3:
         size = 9;
         break;
      case CG_FIXED3x4:
      case CG_FIXED4x3:
         size = 12;
         break;
      case CG_FIXED4x4:
         size = 16;
         break;
      default:
         size = 0;
         break;
   }
   return size;
}

void _cgRaiseInvalidParam (void *data, const void*v )
{
   (void)data;
}

void _cgRaiseNotMatrixParam (void *data, const void*v )
{
   (void)data;
}

void _cgIgnoreParamIndex (void *data, const void*v, const int index )
{
   (void)data;
   // nothing
}

CgRuntimeParameter* _cgGLTestArrayParameter( CGparameter paramIn, long offset, long nelements )
{
   CgRuntimeParameter* param = rglCgGLTestParameter( paramIn );

   return param;
}

CgRuntimeParameter* _cgGLTestTextureParameter( CGparameter param )
{
   CgRuntimeParameter* ptr = rglCgGLTestParameter( param );
   return ptr;
}


#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_enum, nrows, ncols,classname) \
   nrows ,
static int _typesRowCount[] =
{
#include "Cg/cg_datatypes.h"
};

#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_enum, nrows, ncols,classname) \
   ncols ,
static int _typesColCount[] =
{
#include "Cg/cg_datatypes.h"
};

unsigned int rglGetTypeRowCount( CGtype parameterType )
{
   int typeIndex = parameterType - 1 - CG_TYPE_START_ENUM;
   return _typesRowCount[typeIndex];
}

unsigned int rglGetTypeColCount( CGtype parameterType )
{
   int typeIndex = parameterType - 1 - CG_TYPE_START_ENUM;
   return _typesColCount[typeIndex];
}

/*============================================================
  CG PARAMETER
  ============================================================ */

static const CGenum var_table[] = {CG_VARYING, CG_UNIFORM, CG_CONSTANT, CG_MIXED};
static const CGenum dir_table[] = {CG_IN, CG_OUT, CG_INOUT, CG_ERROR};

RGL_EXPORT CgparameterHookFunction _cgParameterCreateHook = NULL;
RGL_EXPORT CgparameterHookFunction _cgParameterDestroyHook = NULL;

static CGparameter rglAdvanceParameter( CGparameter param, int distance )
{
   CgRuntimeParameter *rtParameter = ( CgRuntimeParameter* )rglCgGLTestParameter( param );
   if ( !rtParameter )
      return NULL;

   if ( distance == 0 )
      return param;

   if ( rtParameter > rtParameter->program->runtimeParameters )
   {
      CgRuntimeParameter *previousParameter = rtParameter - 1;
      if (( previousParameter->parameterEntry->flags & CGP_ARRAY ) &&
            !( previousParameter->parameterEntry->flags & CGP_UNROLLED ) )
      {
         int arrayIndex = CG_GETINDEX( param );
         arrayIndex += distance;
         const CgParameterArray *parameterArray =  rglGetParameterArray( previousParameter->program, previousParameter->parameterEntry );
         int arraySize = rglGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount );
         if ( arrayIndex < arraySize )
         {
            int ret = ( int )rtParameter->id;
            ret |= ( arrayIndex << CG_PARAMETERSIZE );
            return ( CGparameter )ret;
         }
         else
         {
            return ( CGparameter )NULL;
         }
      }
   }

   //stop when the distance is 0
   const CgParameterEntry *endEntry = rtParameter->program->parametersEntries + rtParameter->program->rtParametersCount;
   const CgParameterEntry *paramEntry = rtParameter->parameterEntry;

   while ( distance && paramEntry < endEntry )
   {
      switch ( paramEntry->flags & CGP_TYPE_MASK )
      {
         case CGP_ARRAY:
            {
               if ( paramEntry->flags & CGP_UNROLLED )
               {
                  const CgParameterArray *parameterArray = rglGetParameterArray( rtParameter->program, paramEntry );
                  int arraySize = ( int )rglGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount );

                  paramEntry++;
                  distance--;

                  paramEntry++;
                  distance += arraySize;
               }
               else
               {
                  paramEntry++;
                  distance--;
                  paramEntry++;
               }
               break;
            }
         case CGP_STRUCTURE:
            {
               const CgParameterStructure *parameterStructure = rglGetParameterStructure( rtParameter->program, paramEntry );
               paramEntry++;
               distance--;
               distance += parameterStructure->memberCount;
               break;
            }
         case CGP_INTRINSIC:
            paramEntry++;
            distance--;
            break;
      }
   }

   if ( paramEntry < endEntry )
   {
      size_t offset = ( paramEntry - rtParameter->parameterEntry );
      CgRuntimeParameter *nextParameter = rtParameter + offset;
      return nextParameter->id;
   }
   else
   {
      return ( CGparameter )NULL;
   }
}

void rglCgDestroyContextParam( CgRuntimeParameter* ptr )
{
   if ( _cgParameterDestroyHook ) _cgParameterDestroyHook( ptr );

   rglEraseName( &_CurrentContext->cgParameterNameSpace, (unsigned int)( ptr->id ) );

   free( ptr );
}

static int rglGetSizeofSubArray( const short *dimensions, int count )
{
   int res = 1;
   for ( int i = 0;i < count;i++ )
      res *= ( int )( *( dimensions++ ) );
   return res;
}


static _CGparameter *_cgGetNamedParameter( _CGprogram* progPtr, const char* name, CGenum name_space, int *arrayIndex, const CgParameterEntry *_startEntry = NULL, int _entryCount = -1 )
{
   if ( name == NULL )
      return NULL;

   *arrayIndex = -1;
   int done = 0;
   const char *structureEnd;
   const char *structureStart = name;
   int itemIndex = -1;
   _CGprogram *program = progPtr;

   const CgParameterEntry *currentEntry;
   const CgParameterEntry *lastEntry;

   int containerCount = -2;
   if ( _startEntry && _entryCount != -1 )
   {
      currentEntry = _startEntry;
      containerCount = _entryCount;
   }
   else
   {
      currentEntry = program->parametersEntries;
   }
   lastEntry = program->parametersEntries + program->rtParametersCount;

   int isInArray = 0;
   bool bWasUnrolled = false;
   const char *prevStructureStart = structureStart; //for unrolled

   while (( !done ) && ( *structureStart ) && ( containerCount != -1 ) )
   {
      structureEnd = strpbrk( structureStart, ".[" );
      if ( structureEnd == NULL )
      {
         structureEnd = structureStart + strlen( structureStart );
         done = 1;
      }

      if ( bWasUnrolled )
      {
         bWasUnrolled = false;
         structureStart = prevStructureStart;
      }
      char structName[256];
      int length = ( int )( structureEnd - structureStart );
      strncpy( structName, structureStart, length );
      structName[length] = '\0';
      prevStructureStart = structureStart; //for unrolled
      structureStart = structureEnd + 1;

      bool found = false;
      while ( !found && currentEntry < lastEntry && ( containerCount == -2 || containerCount > 0 ) )
      {
         if ( !strncmp( structName, program->stringTable + currentEntry->nameOffset, length )
               && ( name_space == 0 || ( name_space == CG_GLOBAL && ( currentEntry->flags & CGPF_GLOBAL ) )
                  || ( name_space == CG_PROGRAM && !( currentEntry->flags & CGPF_GLOBAL ) ) ) )
         {
            if (( int )strlen( program->stringTable + currentEntry->nameOffset ) != length )
            {
               if ( !strcmp( name, program->stringTable + currentEntry->nameOffset ) )
               {
                  found = true;
                  done = 1;
               }

               if ( !strncmp( name, program->stringTable + currentEntry->nameOffset, length ) &&
                     !strcmp( "[0]", program->stringTable + currentEntry->nameOffset + length ) )
               {
                  found = true;
                  done = 1;
               }
            }
            else
               found = true;
         }

         if ( !found )
         {
            //we are skipping entries here, the search stays at the same level
            int skipCount = 1;
            while ( skipCount && currentEntry < lastEntry )
            {
               //skip as many entry as necessary
               if ( currentEntry->flags & CGP_STRUCTURE )
               {
                  const CgParameterStructure *parameterStructure = rglGetParameterStructure( program, currentEntry );
                  skipCount += parameterStructure->memberCount;
               }
               else if ( currentEntry->flags & CGP_ARRAY )
               {
                  if ( currentEntry->flags & CGP_UNROLLED )
                  {
                     const CgParameterArray *parameterArray =  rglGetParameterArray( program, currentEntry );
                     skipCount += rglGetSizeofSubArray(( short* )parameterArray->dimensions, parameterArray->dimensionCount );
                  }
                  else
                     skipCount++; //the following item will be the type ( can't be a structure as they are always enrolled at the moment )
               }
               currentEntry++;
               skipCount--;
            }
         }
         if ( containerCount != -2 ) //denotes that we are in a structure or in an array and that we shouldn't look beyond
            containerCount--;
      }
      //at that point we failed or we succeeded to find the entry at that level.
      //if we have succeed we continue in the lower level if needed. if we have failed, we return not found.

      if ( found )
      {
         switch ( currentEntry->flags & CGP_TYPE_MASK ) {
            case 0:
               itemIndex = ( int )( currentEntry - program->parametersEntries );
               break;
            case CGP_ARRAY:
               {

                  const CgParameterEntry *arrayEntry = currentEntry;
                  const CgParameterArray *parameterArray =  rglGetParameterArray( program, arrayEntry );

                  if ( *structureEnd == '\0' )  //if we are asked for the array parameter itself and not an array item:
                  {
                     itemIndex = ( int )( currentEntry - program->parametersEntries );
                     break;
                  }

                  //go inside:
                  currentEntry++;
                  if ( currentEntry->flags &CGP_STRUCTURE )
                  {
                     //continue to look into the list, but we should stop before the end of the array
                     bWasUnrolled = true;
                     containerCount = rglGetSizeofSubArray(( short* )parameterArray->dimensions, parameterArray->dimensionCount );
                     break;
                  }
                  else
                  {
                     //the array contains base types
                     isInArray = 1;
                     const char *arrayStart = structureEnd;
                     const char *arrayEnd = structureEnd;

                     int dimensionCount = 0;
                     int arrayCellIndex = 0;
                     while ( *arrayStart == '[' && dimensionCount < parameterArray->dimensionCount )
                     {
                        arrayEnd = strchr( arrayStart + 1, ']' );
                        int length = ( int )( arrayEnd - arrayStart - 1 );
                        char indexString[16];
                        strncpy( indexString, arrayStart + 1, length );
                        indexString[length] = '\0';
                        int index = atoi( indexString );
                        int rowSize = parameterArray->dimensions[dimensionCount];
                        if ( index >= rowSize )
                        {
                           //index out of range: not found
                           return NULL;
                        }
                        arrayCellIndex += index * rglGetSizeofSubArray(( short* )parameterArray->dimensions + dimensionCount, parameterArray->dimensionCount - dimensionCount - 1 );

                        arrayStart = arrayEnd + 1;
                        dimensionCount++;
                     }
                     structureEnd = arrayStart;
                     if ( *structureEnd == '\0' )
                        done = 1;

                     if ( done )
                     {
                        //found the item in the array
                        ( *arrayIndex ) = arrayCellIndex;
                        itemIndex = ( int )( currentEntry - program->parametersEntries );
                     }
                     //else... don't know what to do, it's a array of structure not unrolled
                  }
               }
               break;
            case CGP_STRUCTURE:
               if ( done )
               {
                  //the user is getting back a strut... not sure that all the checks are in place for such a return value
                  itemIndex = ( int )( currentEntry - program->parametersEntries );
               }
               else
               {
                  const CgParameterStructure *parameterStructure = rglGetParameterStructure( program, currentEntry );
                  containerCount = parameterStructure->memberCount;
               }
               break;
            default:
               break;
         }
      }
      if ( found )
      {
         if ( !bWasUnrolled )
            currentEntry++;
      }
      else
         break;//not found
   }

   //have we found it ?
   if ( itemIndex != -1 )
      return ( _CGparameter* )( program->runtimeParameters + itemIndex );
   else
      return NULL;
}

// API functions ----------------------------------------

CG_API CGparameter cgGetNamedParameter( CGprogram prog, const char* name )
{
   // check program handle
   if ( !CG_IS_PROGRAM( prog ) )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return ( CGparameter )NULL;
   }

   _CGprogram* progPtr = _cgGetProgPtr( prog );
   // 0 means don't care
   int arrayIndex = -1;
   CgRuntimeParameter *param = ( CgRuntimeParameter * )_cgGetNamedParameter( progPtr, name, ( CGenum )0, &arrayIndex );
   if ( param )
   {
      int ret = ( int )param->id;
      if ( arrayIndex != -1 )
         ret |= ( arrayIndex << CG_PARAMETERSIZE );
      return ( CGparameter )ret;
   }
   else
      return ( CGparameter )NULL;
}

CG_API CGparameter cgGetFirstParameter( CGprogram prog, CGenum name_space )
{
   // check program handle
   if ( !CG_IS_PROGRAM( prog ) )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return ( CGparameter )NULL;
   }

   _CGprogram* progPtr = _cgGetProgPtr( prog );

   CgRuntimeParameter *rtParameter = ( CgRuntimeParameter* )progPtr->runtimeParameters;

   // find the first param of the requested namespace
   for ( int i = 0; i < progPtr->rtParametersCount; ++i )
   {
      // check parameter handle
      bool isGlobal = ( rtParameter->parameterEntry->flags & CGPF_GLOBAL ) == CGPF_GLOBAL;
      if ( isGlobal && name_space == CG_GLOBAL )
      {
         return ( CGparameter )rtParameter->id;
      }
      if ( !isGlobal && name_space == CG_PROGRAM )
      {
         return ( CGparameter )rtParameter->id;
      }
      rtParameter++;
   }
   return ( CGparameter )NULL;
}

CG_API CGparameter cgGetNextParameter( CGparameter param )
{
   //check parameter handle
   CgRuntimeParameter *rtParameter = ( CgRuntimeParameter* )rglCgGLTestParameter( param );
   if ( !rtParameter )
      return ( CGparameter )NULL;

   // the case of the array element of a compact array is easy to solve
   int arrayIndex = -1;
   if ( rtParameter > rtParameter->program->runtimeParameters )
   {
      CgRuntimeParameter *previousParameter = rtParameter - 1;
      if (( previousParameter->parameterEntry->flags & CGP_ARRAY ) &&
            !( previousParameter->parameterEntry->flags & CGP_UNROLLED ) )
      {
         //we have an array item
         arrayIndex = CG_GETINDEX( param );
         const CgParameterArray *parameterArray =  rglGetParameterArray( previousParameter->program, previousParameter->parameterEntry );
         int arraySize = rglGetSizeofSubArray(( short* )parameterArray->dimensions, parameterArray->dimensionCount );
         arrayIndex++;
         if ( arrayIndex < arraySize )
         {
            int ret = ( int )rtParameter->id;
            ret |= ( arrayIndex << CG_PARAMETERSIZE );
            return ( CGparameter )ret;
         }
         else
            return ( CGparameter )NULL;
      }
   }

   //get the parent of the current as well as its location
   const CgParameterEntry *paramEntry = rtParameter->parameterEntry;
   const CgParameterEntry *firstEntry = rtParameter->program->parametersEntries;

   int distance = 1;
   paramEntry--;

   bool bNextExists = false;
   while ( paramEntry >= firstEntry && !bNextExists )
   {
      switch ( paramEntry->flags & CGP_TYPE_MASK )
      {
         case CGP_ARRAY:
            distance--; // the array has one extra item, whether it's a structure or if it's an intrinsic type
            //Ced: not it's not true, if it's unrolled there is no extra item
            break;
         case CGP_STRUCTURE:
            {
               const CgParameterStructure *parameterStructure = rglGetParameterStructure( rtParameter->program, paramEntry );
               //+1  because of the structure element, there's structure
               if ( distance >= ( parameterStructure->memberCount + 1 ) )
               {
                  //the parameter is not in this structure, so I need to remove from the distance all the structure item
                  //so this structure will count for a distance of just one ( the distance is the level distance )
                  distance -= parameterStructure->memberCount;
               }
               else
               {
                  //We are going to exit here
                  //ok so we were in this structure, so check if we have more item in the structure after the current one
                  if ( distance < parameterStructure->memberCount )
                  {
                     //we still have some items in the structure, take the next item
                     bNextExists = true;
                     break;
                  }
                  else
                  {
                     //no more items at this level return null
                     return ( CGparameter )NULL;
                  }
               }
               break;
            }
         case CGP_INTRINSIC:
            break;
      }
      distance++;
      paramEntry--;
   }
   //we have now elimated the case where we were in a struct and there were no more items, now if the item has a successor,
   //this is what we are looking for

   //we have already treated the current entry at that point, the loop starts on the previous one, the distance is 1
   CGparameter nextParam = rglAdvanceParameter( rtParameter->id, 1 );

   if ( nextParam )
   {
      CgRuntimeParameter *nextParameter = ( CgRuntimeParameter* )rglCgGLTestParameter( nextParam );

      //check that next param is still in the same namespace as param
      bool isCurrentGlobal = ( rtParameter->parameterEntry->flags & CGPF_GLOBAL ) == CGPF_GLOBAL;
      bool isNextGlobal = ( nextParameter->parameterEntry->flags & CGPF_GLOBAL ) == CGPF_GLOBAL;
      if ( isNextGlobal != isCurrentGlobal )
      {
         //the next item doesn't have the same global flag... since they are grouped, it means we have quitted the current struct
         return ( CGparameter )NULL;
      }
      else
         return nextParam;
   }
   else
   {
      //case where we were at the root and there is no more items
      return ( CGparameter )NULL;
   }
}

CG_API CGparameter cgGetFirstStructParameter( CGparameter param )
{
   //check parameter handle
   CgRuntimeParameter *rtParameter = ( CgRuntimeParameter* )rglCgGLTestParameter( param );
   if ( !rtParameter )
      return ( CGparameter )NULL;

   const CgParameterEntry *parameterEntry = rtParameter->parameterEntry;
   if (( parameterEntry->flags & CGP_STRUCTURE ) != CGP_STRUCTURE )
   {
      // failure, there was no child.
      rglCgRaiseError( CG_INVALID_PARAM_HANDLE_ERROR );
      return ( CGparameter )NULL;
   }

   const CgParameterStructure *parameterStructure = rglGetParameterStructure( rtParameter->program, parameterEntry );
   if ( parameterStructure->memberCount > 0 ) //is is needed ?
   {
      CgRuntimeParameter *firstStructureItem = rtParameter + 1;
      return firstStructureItem->id;
   }
   else
      return ( CGparameter )NULL; //we have a struct with 0 items ?
}

CG_API CGparameter cgGetArrayParameter( CGparameter param, int arrayIndex )
{
   // check parameter handle
   CgRuntimeParameter *rtParameter = ( CgRuntimeParameter* )_cgGLTestArrayParameter( param, arrayIndex, 1 );
   if ( !rtParameter )
      return ( CGparameter )NULL;

   CGparameter arrayFirstItemID;
   if ( rtParameter->parameterEntry->flags & CGP_UNROLLED )
   {
      //move over the first item of the array and starts from here
      rtParameter++;
      arrayFirstItemID = rtParameter->id;
   }
   else
   {
      // SPECIAL CASE FOR ARRAY_OF_SAMPLERS
      if ( RGL_UNLIKELY( !( rtParameter->parameterEntry->flags & ( CGP_STRUCTURE | CGP_ARRAY ) ) ) &&
            isSampler( rglGetParameterCGtype( rtParameter->program, rtParameter->parameterEntry ) ) )
      {
         for ( int i = 0; i < arrayIndex; ++i )
         {
            rtParameter++;
         }
         return rtParameter->id;
      }
      // END SPECIAL CASE FOR ARRAY_OF_SAMPLERS

      //move to the type item
      rtParameter++;
      //and create the ID for the item 0
      //I know this is stupid, and that this really the same as the previous case, but that's to make the code understandable
      arrayFirstItemID = ( CGparameter )((( unsigned int )rtParameter->id ) | ( 0 << CG_PARAMETERSIZE ) );
   }
   CGparameter arrayItemID = rglAdvanceParameter( arrayFirstItemID, arrayIndex );
   return arrayItemID;
}

CG_API int cgGetArraySize( CGparameter param, int dimension )
{
   // check parameter handle
   CgRuntimeParameter *rtParameter = ( CgRuntimeParameter* )rglCgGLTestParameter( param );
   if ( !rtParameter )
      return -1;

   // this is another rarely queried value (see also cgGetArrayDimension), so we
   // do not store it. Instead we calculate it every time it is requested.
   // recurse down the array tree decrementing "dimension" until either it reached zero
   // and we return the arraySize or we fail to find a child in which case the
   // dimension was invalid.

   const CgParameterEntry *parameterEntry = rtParameter->parameterEntry;
   if (( parameterEntry->flags & CGP_ARRAY ) == 0 )
   {
      // ***** NOT IN CG DOCUMENTATION, but should be ****
      rglCgRaiseError( CG_ARRAY_PARAM_ERROR );
      return CG_UNKNOWN_TYPE;
   }
   else
   {
      const CgParameterArray *parameterArray = rglGetParameterArray( rtParameter->program, parameterEntry );
      if ( dimension < 0 || dimension >= parameterArray->dimensionCount )
      {
         rglCgRaiseError( CG_INVALID_DIMENSION_ERROR );
         return -1;
      }
      else
      {
         return ( int )parameterArray->dimensions[dimension];
      }
   }
   return -1;
}

CG_API CGprogram cgGetParameterProgram( CGparameter param )
{
   // check parameter handle
   CgRuntimeParameter *rtParameter = ( CgRuntimeParameter* )rglCgGLTestParameter( param );
   if ( !rtParameter )
   {
      return ( CGprogram )NULL;
   }
   else if ( RGL_UNLIKELY( rtParameter->parameterEntry->flags & CGP_RTCREATED ) )
   {
      // I don't think we want to expose the fact that we internally store runtime created effect and context parameters in a program
      return ( CGprogram )NULL;
   }
   else
   {
      return rtParameter->program->id;
   }
   return ( CGprogram )NULL;
}

CG_API CGcontext cgGetParameterContext( CGparameter param )
{
   // check parameter handle
   if ( !CG_IS_PARAMETER( param ) )
   {
      rglCgRaiseError( CG_INVALID_PARAM_HANDLE_ERROR );
      return ( CGcontext )NULL;
   }

   CGcontext result = cgGetProgramContext( cgGetParameterProgram( param ) );

   return result;
}

CG_API CGbool cgIsParameter( CGparameter param )
{
   if ( RGL_LIKELY( CG_IS_PARAMETER( param ) ) )
   {
      return CG_TRUE;
   }
   return CG_FALSE;
}

CG_API CGtype cgGetParameterType( CGparameter param )
{
   CgRuntimeParameter *rtParameter = ( CgRuntimeParameter* )rglCgGLTestParameter( param );
   if ( !rtParameter )
      return CG_UNKNOWN_TYPE;
   else
   {
      if ( rtParameter->parameterEntry->flags & CGP_ARRAY )
         return CG_ARRAY;
      else if ( rtParameter->parameterEntry->flags & CGP_STRUCTURE )
         return CG_STRUCT;
      else
      {
         return rglGetParameterCGtype( rtParameter->program, rtParameter->parameterEntry );
      }
   }
}

CG_API CGtype cgGetParameterNamedType( CGparameter param )
{
   return cgGetParameterType( param );
}

CG_API const char* cgGetParameterSemantic( CGparameter param )
{
   // check parameter handle
   CgRuntimeParameter *rtParameter = ( CgRuntimeParameter* )rglCgGLTestParameter( param );
   if ( !rtParameter )
      return NULL;

   const CgParameterEntry *parameterEntry = rtParameter->parameterEntry;

   unsigned short type = parameterEntry->flags & CGP_TYPE_MASK;
   if ( type == CGP_STRUCTURE || CGP_STRUCTURE == CGP_ARRAY )
   {
      rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
      return NULL;
   }

   size_t entryIndex = ( parameterEntry - rtParameter->program->parametersEntries );

   //look for the parameter semantic in the semantic table for semantics set in the compiled source
   int count = rtParameter->program->semanticCount;
   int i;
   for ( i = 0;i < count;i++ )
   {
      const CgParameterSemantic *semantic = rtParameter->program->semanticIndices + i;

      if ( semantic->entryIndex == ( unsigned short )entryIndex )
         return rtParameter->program->stringTable + semantic->semanticOffset; // found
   }

   //not found, we don't have the semantic for this parameter, returns empty strings
   return "";
}

static bool rglPrependString( char *dst, const char *src, size_t size )
{
   int len = strlen( src );
   int previousLen = strlen( dst );
   int spaceLeft = size - ( previousLen + 1 ); //+1 for white space
   if ( spaceLeft < len )
   {
      return false;
   }
   memmove( dst + len, dst, previousLen + 1 );
   strncpy( dst, src, len );
   return true;
}

CG_API const char* cgGetParameterName( CGparameter param )
{
   // check parameter handle
   CgRuntimeParameter *rtParameter = ( CgRuntimeParameter* )rglCgGLTestParameter( param );
   if ( !rtParameter )
      return NULL;

   // runtime created parameters have their names stored in the entry differently than compile created params
   if ( rtParameter->parameterEntry->flags & CGP_RTCREATED )
   {
      return reinterpret_cast<const char*>( rtParameter->parameterEntry->nameOffset );
   }

   char *currentParameterName = rtParameter->program->parentContext->currentParameterName;
   currentParameterName[0] = '\0';
   size_t stringSize = sizeof( rtParameter->program->parentContext->currentParameterName );

   //I walk down the parameterEntry list until I find the root
   const CgParameterEntry *paramEntry = rtParameter->parameterEntry;
   const CgParameterEntry *firstEntry = rtParameter->program->parametersEntries;

   //start by the current name
   bool res = rglPrependString( currentParameterName, rtParameter->program->stringTable + paramEntry->nameOffset, stringSize );
   if ( !res )
      return NULL;
   //are we starting from an array ?
   if ( paramEntry > firstEntry )
   {
      const CgParameterEntry *previousEntry = paramEntry - 1;
      if (( previousEntry->flags & CGP_ARRAY ) && !( previousEntry->flags & CGP_UNROLLED ) )
      {
         //ok we are in an non unrolled array
         //I need to append the array index, I should use the dimensions , no time for now
         int index = CG_GETINDEX( param );
         //should divide the index on the dimensions... later...
         char buffer[256];
         sprintf( buffer, "[%i]", index );
         if ( strlen( currentParameterName ) + strlen( buffer ) + 1 < stringSize )
            strcat( currentParameterName, buffer );
         else
         {
            return NULL;
         }
         //prepend array name
         res = rglPrependString( currentParameterName, rtParameter->program->stringTable + previousEntry->nameOffset, stringSize );
         if ( !res )
            return NULL;
         paramEntry--;
      }
   }

   //we have already treated the current entry at that point, the loop starts on the previous one, the distance is 1
   int distance = 1;
   paramEntry--;

   while ( paramEntry >= firstEntry )
   {
      switch ( paramEntry->flags & CGP_TYPE_MASK )
      {
         case CGP_ARRAY:
            distance--; // the array has one extra item, whether it's a structure or if it's an intrinsic type
            break;
         case CGP_STRUCTURE:
            {
               const CgParameterStructure *parameterStructure = rglGetParameterStructure( rtParameter->program, paramEntry );
               if ( distance > parameterStructure->memberCount )
               {
                  //the parameter is not in this structure, so I need to remove from the distance all the structure item
                  distance -= parameterStructure->memberCount;
               }
               else
               {
                  //the parameter is in this structure, prepend the name
                  res = rglPrependString( currentParameterName, ".", stringSize );
                  if ( !res ) return NULL;
                  res = rglPrependString( currentParameterName, rtParameter->program->stringTable + paramEntry->nameOffset, stringSize );
                  if ( !res ) return NULL;
                  distance = 0;
               }
               break;
            }
         case CGP_INTRINSIC:
            break;
      }
      distance++;
      paramEntry--;
   }
   return currentParameterName;

}

CG_API CGenum cgGetParameterVariability( CGparameter param )
{
   // check parameter handle
   CgRuntimeParameter *rtParameter = ( CgRuntimeParameter* )rglCgGLTestParameter( param );
   if ( !rtParameter )
      return ( CGenum )0;
   else
   {
      unsigned int variability = rtParameter->parameterEntry->flags & CGPV_MASK;
      if ( variability == CGPV_UNIFORM )
         return CG_UNIFORM;
      else if ( variability == CGPV_VARYING )
         return CG_VARYING;
      else if ( variability == CGPV_CONSTANT )
         return CG_CONSTANT;
      else if ( variability == CGPV_MIXED )
         return CG_MIXED;
      else
         return ( CGenum )0;
   }
}

CG_API CGenum cgGetParameterDirection( CGparameter param )
{
   // check parameter handle
   CgRuntimeParameter *rtParameter = ( CgRuntimeParameter* )rglCgGLTestParameter( param );
   if ( !rtParameter )
      return CG_ERROR;
   else
   {
      unsigned int direction = rtParameter->parameterEntry->flags & CGPD_MASK;
      if ( direction == CGPD_IN )
         return CG_IN;
      else if ( direction == CGPD_OUT )
         return CG_OUT;
      else if ( direction == CGPD_INOUT )
         return CG_INOUT;
      else
         return CG_ERROR;
   }
}

/*============================================================
  CG TOKENS
  ============================================================ */

typedef struct RGLcgProfileMapType
{
   CGprofile id;
   char* string;
   int is_vertex_program;
}
RGLcgProfileMapType;

typedef struct RGLcgTypeMapType
{
   CGtype id;
   char* string;
   CGparameterclass parameter_class;
}
RGLcgTypeMapType;

// string tables ------------------------
// string tables map enum values to printable strings

static const RGLcgTypeMapType RGLcgTypeMap[] =
{
   { CG_UNKNOWN_TYPE, "unknown", CG_PARAMETERCLASS_UNKNOWN},
   { CG_STRUCT, "struct", CG_PARAMETERCLASS_STRUCT},
   { CG_ARRAY, "array", CG_PARAMETERCLASS_ARRAY},

   //NOTE: string is compiler friendly lower-case version of type name
#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_enum, nrows, ncols,classname) \
   { enum_name, #compiler_name, classname },
#include <Cg/cg_datatypes.h>

   // note: assumes CG_UNKNOWN_TYPE == 0
   { CGtype( 0 ), "", CG_PARAMETERCLASS_UNKNOWN }
};

// templated map lookup functions  ---------


   template<typename T, size_t N>
const T*rglLookupEnum( const T( &map )[N], const int key )
{
   const unsigned int count = sizeof( map ) / sizeof( map[0] );
   for ( unsigned int i = 0; i < count; ++i )
   {
      if ( map[i].id == key )
      {
         return map + i;
      }
   }
   return 0;
}


   template<typename T, size_t N>
const T* rglLookupString( const T( &map )[N], const char *key )
{
   const unsigned int count = sizeof( map ) / sizeof( map[0] );
   if (key != NULL)
   {
      for ( unsigned int i = 0; i < count; ++i )
      {
         if (strcmp( map[i].string, key) == 0)
            return map + i;
      }
   }
   return 0;
}






// API type functions -----------------------

CG_API const char* cgGetTypeString( CGtype type )
{
   const RGLcgTypeMapType *ptr = rglLookupEnum( RGLcgTypeMap, type );
   if (ptr != 0)
      return const_cast<const char*>( ptr->string );
   return "";
}

CG_API CGtype cgGetType( const char* type_string )
{
   const RGLcgTypeMapType *ptr = rglLookupString( RGLcgTypeMap, type_string );
   if (ptr != 0)
      return static_cast<CGtype>( ptr->id );
   return CG_UNKNOWN_TYPE;
}


// User type functions ----------------------

CG_API CGtype cgGetNamedUserType( CGhandle handle, const char* name )
{
   // TODO **************
   return CG_UNKNOWN_TYPE;
}

CG_API int cgGetNumUserTypes( CGhandle handle )
{
   // TODO **************
   return 0;
}

CG_API CGtype cgGetUserType( CGhandle handle, int index )
{
   // TODO **************
   return CG_UNKNOWN_TYPE;
}

CG_API int cgGetNumParentTypes( CGtype type )
{
   // TODO **************
   return 0;
}

CG_API CGtype cgGetParentType( CGtype type, int index )
{
   // TODO **************
   return CG_UNKNOWN_TYPE;
}

CG_API CGbool cgIsParentType( CGtype parent, CGtype child )
{
   // TODO **************
   return CG_FALSE;
}

CG_API CGbool cgIsInterfaceType( CGtype type )
{

   // TODO **************
   return CG_FALSE;
}

// Profile functions -------------------------


// ErrorFunctions ----------------------------
CG_API CGerror cgGetError( void )
{
   CGerror err = _CurrentContext->RGLcgLastError;
   _CurrentContext->RGLcgLastError = CG_NO_ERROR;
   return err;
}

CG_API const char* cgGetErrorString( CGerror error )
{
   return "cgGetErrorString not implemented.\n";
}

CG_API void cgSetErrorCallback( CGerrorCallbackFunc func )
{
   _CurrentContext->RGLcgErrorCallbackFunction = func;
}

CG_API CGerrorCallbackFunc cgGetErrorCallback( void )
{
   return _CurrentContext->RGLcgErrorCallbackFunction;
}


// Misc Functions -----------------------------

CG_API const char* cgGetString( CGenum sname )
{
   if ( sname == CG_VERSION )
   {
      // this should return the version of the runtime or perhaps more meaninfully, the runtime compiler
      static char versionstring[8];
      sprintf( versionstring, "%d", CG_VERSION_NUM );
      return versionstring;
   }

   rglCgRaiseError( CG_INVALID_ENUMERANT_ERROR );
   return NULL;
}

CG_API CGparameterclass cgGetTypeClass( CGtype type )
{
   const RGLcgTypeMapType *ptr = rglLookupEnum( RGLcgTypeMap, type );
   if (ptr == 0)
      return CG_PARAMETERCLASS_UNKNOWN;
   return ptr->parameter_class;
}

CG_API CGtype cgGetTypeBase( CGtype type )
{
   // get the base type of a usertype without having to create
   // a parameter of that type.

   // TODO: usertypes not supported in Jetstream
   return CG_UNKNOWN_TYPE;
}

/*============================================================
  CG CONTEXT
  ============================================================ */

RGL_EXPORT CgcontextHookFunction _cgContextCreateHook = NULL;
RGL_EXPORT CgcontextHookFunction _cgContextDestroyHook = NULL;

void rglCgContextZero( _CGcontext* p )
{
   memset( p, 0, sizeof( *p ) );
   p->compileType = CG_UNKNOWN;

}

void rglCgContextPushFront( _CGcontext* ctx )
{
   if ( _CurrentContext->RGLcgContextHead )
   {
      _CGcontext* head = _cgGetContextPtr( _CurrentContext->RGLcgContextHead );
      // insert this context at the head of the list
      ctx->next = head;
   }
   _CurrentContext->RGLcgContextHead = ctx->id;
}

static void destroy_context( _CGcontext*ctx )
{
   if ( _cgContextDestroyHook ) _cgContextDestroyHook( ctx );
   // free the id
   rglEraseName( &_CurrentContext->cgContextNameSpace, (unsigned int)ctx->id );
   // zero the memory
   rglCgContextZero( ctx );
   // return context to free store
   free( ctx );
}

void rglCgContextPopFront()
{
   // remove and delete the context at the head of the list
   if ( _CurrentContext->RGLcgContextHead )
   {
      _CGcontext* head = _cgGetContextPtr( _CurrentContext->RGLcgContextHead );
      _CGcontext* temp = head->next;
      // free the id as well
      destroy_context( head );

      if ( temp )
      {
         // this is not the end of the list, feel free to dereference it.
         _CurrentContext->RGLcgContextHead = temp->id;
      }
      else
      {
         // nothing left, no dereferenceing for you, mister.
         _CurrentContext->RGLcgContextHead = 0;
      }
   }
}


void rglCgContextEraseAfter( _CGcontext* c )
{
   _CGcontext* eraseme = c->next;
   c->next = eraseme->next;

   destroy_context( eraseme );
}

// API functions ----------------------------------------

CG_API CGcontext cgCreateContext( void )
{
   // create a context out of thin air and add it to the hidden global list.

   _CGcontext* ptr = NULL;

   // alloc new context
   ptr = ( _CGcontext* )malloc( sizeof( _CGcontext ) );
   if ( NULL == ptr )
   {
      rglCgRaiseError( CG_MEMORY_ALLOC_ERROR );
      return ( CGcontext )NULL;
   }

   // initialise member variables
   rglCgContextZero( ptr );

   // get a new id for the object
   CGcontext result = ( CGcontext )rglCreateName( &_CurrentContext->cgContextNameSpace, ptr );
   if ( !result ) { free( ptr ); return NULL; }

   ptr->id = result;
   ptr->defaultProgram.parentContext = ptr;

   // insert context at head of global list
   rglCgContextPushFront( ptr );

   if ( _cgContextCreateHook ) _cgContextCreateHook( ptr );

   return result;
}

CG_API void cgDestroyContext( CGcontext c )
{
   // check if context is valid
   if ( !CG_IS_CONTEXT( c ) )
   {
      rglCgRaiseError( CG_INVALID_CONTEXT_HANDLE_ERROR );
      return;
   }

   _CGcontext* ctx = _cgGetContextPtr( c );

   rglCgProgramErase( &ctx->defaultProgram );

   // destroy all programs
   rglCgProgramDestroyAll( ctx );

   // unlink from global CGContext list
   _CGcontext * const head = _cgGetContextPtr( _CurrentContext->RGLcgContextHead );
   if ( head != ctx )
   {
      // node is not the head, find and erase it

      // find the context that occurs before this one
      _CGcontext* ptr = head;
      while ( ptr->next != ctx ) ptr = ptr->next;
      // relink
      ptr->next = ctx->next;
      destroy_context( ctx );
   }
   else
   {

      // node is the head, erase it
      _CGcontext* second = head->next;
      destroy_context( head );

      if ( second )
      {
         // link to second element
         _CurrentContext->RGLcgContextHead = second->id;
      }
      else
      {
         // nothing left
         _CurrentContext->RGLcgContextHead = 0;
      }
   }
}

CG_API CGbool cgIsContext( CGcontext ctx )
{
   // is the pointer valid?
   if ( CG_IS_CONTEXT( ctx ) )
   {
      return CG_TRUE;
   }
   return CG_FALSE;
}

CG_API const char* cgGetLastListing( CGcontext c )
{
   // check to see if context is a valid one
   // NOTE: Cg API Reference docs omit this test for this API function.
   if ( !CG_IS_CONTEXT( c ) )
   {
      rglCgRaiseError( CG_INVALID_CONTEXT_HANDLE_ERROR );
      return NULL;
   }

   // currently does nothing
   // TODO ****************************

   return NULL;
}


CG_API void cgSetAutoCompile( CGcontext c, CGenum flag )
{
   // check to see if context is a valid one
   if ( !CG_IS_CONTEXT( c ) )
   {
      rglCgRaiseError( CG_INVALID_CONTEXT_HANDLE_ERROR );
      return;
   }

   // check if enum has any meaning here
   switch ( flag )
   {
      case CG_COMPILE_MANUAL:
      case CG_COMPILE_IMMEDIATE:
      case CG_COMPILE_LAZY:
         // set the value and return
         _cgGetContextPtr( c )->compileType = flag;
         break;
      default:
         rglCgRaiseError( CG_INVALID_ENUMERANT_ERROR );
   }
}

/*============================================================
  CG PROGRAM
  ============================================================ */

RGL_EXPORT CgprogramHookFunction _cgProgramCreateHook = NULL;
RGL_EXPORT CgprogramHookFunction _cgProgramDestroyHook = NULL;
RGL_EXPORT CgprogramCopyHookFunction _cgProgramCopyHook = NULL;

RGL_EXPORT cgRTCgcCompileHookFunction _cgRTCgcCompileProgramHook = NULL;
RGL_EXPORT cgRTCgcFreeHookFunction _cgRTCgcFreeCompiledProgramHook;

// Program list functions
// The context can contain many programs, but each one is a variable sized
// chunk of data produced by the CGC compiler and loaded at runtime. We cannot
// include intrusive "next" pointers in this data block so instead we'll use
// a non-intrusive list. The list is walked using cgGetFirstProgram() and
// cgGetNextProgram()

void rglCgProgramZero( _CGprogram* p )
{
   // zero all pointers in the node and enclosed binary program
   // this makes sure cgIsProgram calls on invalid pointers always fail
   memset( p, 0, sizeof( _CGprogram ) );
   return;
}

void rglCgProgramPushFront( _CGcontext* ctx, _CGprogram* prog )
{
   // push the program to the context.
   // updates the linked list and program count

   // link the node into the head of the list
   prog->next = ctx->programList;
   ctx->programList = prog;

   // update the parent context
   prog->parentContext = ctx;

   // update the program count
   ctx->programCount++;
}

_CGprogram* rglCgProgramFindPrev( _CGcontext* ctx, _CGprogram* prog )
{
   // get a pointer the the list element *before* prog in the list
   // this allows us to unlink it safely.

   _CGprogram* ptr = ctx->programList;

   while ( ptr != NULL && prog != ptr->next )
      ptr = ptr->next;

   return ptr;
}

inline void rglCgProgramEraseAndFree( _CGprogram* prog )
{
   rglCgProgramErase( prog );
   free( prog );
}

void rglCgProgramErase( _CGprogram* prog )
{
   // remove the program from the linked list and deallocate it's storage.

   if ( _cgProgramDestroyHook ) _cgProgramDestroyHook( prog );

   switch ( prog->header.profile )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
         //case CG_PROFILE_SCE_VP_TYPEC:
      case CG_PROFILE_SCE_VP_RSX:
      case CG_PROFILE_SCE_FP_TYPEB:
         //case CG_PROFILE_SCE_FP_TYPEC:
      case CG_PROFILE_SCE_FP_RSX:
         rglpProgramErase( prog );
         break;
      default:
         // default program
         break;
   }

   // return program and node to free store
   if ( prog->id ) rglEraseName( &_CurrentContext->cgProgramNameSpace, (unsigned int)prog->id );

   //free ELF data obtained from the runtime converter
   if ( prog->runtimeElf )
      free( prog->runtimeElf );

   // zero out all pointers
   rglCgProgramZero( prog );
}

void rglCgProgramEraseAfter( _CGprogram* prog )
{
   // delete the list entry after the current pointer.
   // (use in conjunction with find_prev for full effect!)

   _CGprogram* next = prog->next;
   if ( next )
   {
      prog->next = next->next;
      rglCgProgramEraseAndFree( next );
   }
}

void rglCgProgramDestroyAll( _CGcontext* ctx )
{
   // destroy all programs in this context
   while ( ctx->programList )
   {
      _CGprogram * p = ctx->programList;
      ctx->programList = p->next;
      // erase the program
      rglCgProgramEraseAndFree( p );
   }
}


static inline unsigned int getMatrixRowCount( CGtype type )
{
   unsigned int rc = 0;
   switch ( type )
   {
      case CG_FLOAT1x1:
      case CG_FLOAT1x2:
      case CG_FLOAT1x3:
      case CG_FLOAT1x4:
         rc = 1;
         break;
      case CG_FLOAT2x1:
      case CG_FLOAT2x2:
      case CG_FLOAT2x3:
      case CG_FLOAT2x4:
         rc = 2;
         break;
      case CG_FLOAT3x1:
      case CG_FLOAT3x2:
      case CG_FLOAT3x3:
      case CG_FLOAT3x4:
         rc = 3;
         break;
      case CG_FLOAT4x1:
      case CG_FLOAT4x2:
      case CG_FLOAT4x3:
      case CG_FLOAT4x4:
         rc = 4;
         break;
      case CG_HALF1x1:
      case CG_HALF1x2:
      case CG_HALF1x3:
      case CG_HALF1x4:
      case CG_INT1x1:
      case CG_INT1x2:
      case CG_INT1x3:
      case CG_INT1x4:
      case CG_BOOL1x1:
      case CG_BOOL1x2:
      case CG_BOOL1x3:
      case CG_BOOL1x4:
      case CG_FIXED1x1:
      case CG_FIXED1x2:
      case CG_FIXED1x3:
      case CG_FIXED1x4:
         rc = 1;
         break;
      case CG_HALF2x1:
      case CG_HALF2x2:
      case CG_HALF2x3:
      case CG_HALF2x4:
      case CG_INT2x1:
      case CG_INT2x2:
      case CG_INT2x3:
      case CG_INT2x4:
      case CG_BOOL2x1:
      case CG_BOOL2x2:
      case CG_BOOL2x3:
      case CG_BOOL2x4:
      case CG_FIXED2x1:
      case CG_FIXED2x2:
      case CG_FIXED2x3:
      case CG_FIXED2x4:
         rc = 2;
         break;
      case CG_HALF3x1:
      case CG_HALF3x2:
      case CG_HALF3x3:
      case CG_HALF3x4:
      case CG_INT3x1:
      case CG_INT3x2:
      case CG_INT3x3:
      case CG_INT3x4:
      case CG_BOOL3x1:
      case CG_BOOL3x2:
      case CG_BOOL3x3:
      case CG_BOOL3x4:
      case CG_FIXED3x1:
      case CG_FIXED3x2:
      case CG_FIXED3x3:
      case CG_FIXED3x4:
         rc = 3;
         break;
      case CG_HALF4x1:
      case CG_HALF4x2:
      case CG_HALF4x3:
      case CG_HALF4x4:
      case CG_INT4x1:
      case CG_INT4x2:
      case CG_INT4x3:
      case CG_INT4x4:
      case CG_BOOL4x1:
      case CG_BOOL4x2:
      case CG_BOOL4x3:
      case CG_BOOL4x4:
      case CG_FIXED4x1:
      case CG_FIXED4x2:
      case CG_FIXED4x3:
      case CG_FIXED4x4:
         rc = 4;
         break;
      default:
         break;
   }
   return rc;
}

CGtype getMatrixRowType( CGtype type )
{
   CGtype rt = ( CGtype )0;
   switch ( type )
   {
      case CG_FLOAT1x1:
      case CG_FLOAT2x1:
      case CG_FLOAT3x1:
      case CG_FLOAT4x1:
         rt = CG_FLOAT1;
         break;
      case CG_FLOAT1x2:
      case CG_FLOAT2x2:
      case CG_FLOAT3x2:
      case CG_FLOAT4x2:
         rt = CG_FLOAT2;
         break;
      case CG_FLOAT1x3:
      case CG_FLOAT2x3:
      case CG_FLOAT3x3:
      case CG_FLOAT4x3:
         rt = CG_FLOAT3;
         break;
      case CG_FLOAT1x4:
      case CG_FLOAT2x4:
      case CG_FLOAT3x4:
      case CG_FLOAT4x4:
         rt = CG_FLOAT4;
         break;
      case CG_HALF1x1:
      case CG_HALF2x1:
      case CG_HALF3x1:
      case CG_HALF4x1:
         rt = CG_HALF1;
         break;
      case CG_HALF1x2:
      case CG_HALF2x2:
      case CG_HALF3x2:
      case CG_HALF4x2:
         rt = CG_HALF2;
         break;
      case CG_HALF1x3:
      case CG_HALF2x3:
      case CG_HALF3x3:
      case CG_HALF4x3:
         rt = CG_HALF3;
         break;
      case CG_HALF1x4:
      case CG_HALF2x4:
      case CG_HALF3x4:
      case CG_HALF4x4:
         rt = CG_HALF4;
         break;
      case CG_INT1x1:
      case CG_INT2x1:
      case CG_INT3x1:
      case CG_INT4x1:
         rt = CG_INT1;
         break;
      case CG_INT1x2:
      case CG_INT2x2:
      case CG_INT3x2:
      case CG_INT4x2:
         rt = CG_INT2;
         break;
      case CG_INT1x3:
      case CG_INT2x3:
      case CG_INT3x3:
      case CG_INT4x3:
         rt = CG_INT3;
         break;
      case CG_INT1x4:
      case CG_INT2x4:
      case CG_INT3x4:
      case CG_INT4x4:
         rt = CG_INT4;
         break;
      case CG_BOOL1x1:
      case CG_BOOL2x1:
      case CG_BOOL3x1:
      case CG_BOOL4x1:
         rt = CG_BOOL1;
         break;
      case CG_BOOL1x2:
      case CG_BOOL2x2:
      case CG_BOOL3x2:
      case CG_BOOL4x2:
         rt = CG_BOOL2;
         break;
      case CG_BOOL1x3:
      case CG_BOOL2x3:
      case CG_BOOL3x3:
      case CG_BOOL4x3:
         rt = CG_BOOL3;
         break;
      case CG_BOOL1x4:
      case CG_BOOL2x4:
      case CG_BOOL3x4:
      case CG_BOOL4x4:
         rt = CG_BOOL4;
         break;
      case CG_FIXED1x1:
      case CG_FIXED2x1:
      case CG_FIXED3x1:
      case CG_FIXED4x1:
         rt = CG_FIXED1;
         break;
      case CG_FIXED1x2:
      case CG_FIXED2x2:
      case CG_FIXED3x2:
      case CG_FIXED4x2:
         rt = CG_FIXED2;
         break;
      case CG_FIXED1x3:
      case CG_FIXED2x3:
      case CG_FIXED3x3:
      case CG_FIXED4x3:
         rt = CG_FIXED3;
         break;
      case CG_FIXED1x4:
      case CG_FIXED2x4:
      case CG_FIXED3x4:
      case CG_FIXED4x4:
         rt = CG_FIXED4;
         break;
      default:
         break;
   }
   return rt;
}


// forward declaration for recursion
void AccumulateSizeForParams( CgParameterEntry* paramEntry, CgParameterEntry* paramEntries, unsigned int paramEntriesCount,
      unsigned short* paramResourceTable, ELF_section_t* strtab, CgParameterEntry** nextEntry,
      unsigned int prefixLength, int elementResourceIndex,
      size_t* nvParamSize, size_t* nvParamStringsSize, size_t* nvParamOffsetsSize,
      CgParameterSemantic* semanticIndexTable, unsigned int semanticIndexCount,
      CgParameterDefaultValue* defaultIndexTable, unsigned int defaultIndexCount, size_t* nvParamDefaultsSize,
      ELF_section_t* consttab );

void AccumulateSizeForParamResource( CgParameterEntry* paramEntry, CgParameterEntry* paramEntries, unsigned int paramEntriesCount,
      unsigned short* paramResourceTable, ELF_section_t* strtab, CgParameterEntry** nextEntry,
      unsigned int prefixLength, int elementResourceIndex,
      size_t* nvParamSize, size_t* nvParamStringsSize, size_t* nvParamOffsetsSize,
      CgParameterSemantic* semanticIndexTable, unsigned int semanticIndexCount,
      CgParameterDefaultValue* defaultIndexTable, unsigned int defaultIndexCount, size_t* nvParamDefaultsSize,
      ELF_section_t* consttab )
{
   // type index is actually the offset into the block of memory following the entries
   CgParameterResource* paramResource = ( CgParameterResource* )(( char* )( paramEntries + paramEntriesCount ) + paramEntry->typeIndex );

   unsigned int rowCount = getMatrixRowCount(( CGtype )paramResource->type );

   // param
   *nvParamSize += sizeof( CgBinaryParameter );
   // and for the row params of a matrix
   *nvParamSize += rowCount * sizeof( CgBinaryParameter );

   // name
   *nvParamStringsSize += ( prefixLength + strlen( strtab->data + paramEntry->nameOffset ) + 1 );
   // and for each row of a matrix which assumes just an extra 3 characters for [i]
   *nvParamStringsSize += ( rowCount * ( prefixLength + strlen( strtab->data + paramEntry->nameOffset ) + 3 + 1 ) );


   // semantic

   // totally inefficient, but easiest implementation for now is to do the On^2 search
   // in the semantic and default tables for the data associated with this parameter
   unsigned int entryIndex = paramEntry - paramEntries;

   // semantic
   char* semantic = NULL;
   for ( unsigned int sem = 0; sem < semanticIndexCount; sem++ )
   {
      CgParameterSemantic* semanticIndex = semanticIndexTable + sem;
      if ( semanticIndex->entryIndex == entryIndex )
      {
         // found the semantic for this parameter
         semantic = strtab->data + semanticIndex->semanticOffset;
         break;
      }
   }
   if ( semantic )
   {
      // increment the names size buffer
      *nvParamStringsSize += strlen( semantic ) + 1;
   }

   // default
   float* defaults = NULL;
   for ( unsigned int def = 0; def < defaultIndexCount; def++ )
   {
      CgParameterDefaultValue* defaultIndex = defaultIndexTable + def;
      if ( defaultIndex->entryIndex == entryIndex )
      {
         // found the default for this parameter
         defaults = ( float* )consttab->data + defaultIndex->defaultValueIndex;
      }
   }
   if ( defaults )
   {
      // defaults are always padded to 4 floats per param
      unsigned int defaultsCount = ( rowCount ? rowCount : 1 ) * 4;
      *nvParamDefaultsSize += sizeof( float ) * defaultsCount;
   }

   // offsets
   // fragment programs
   // do we want this referenced param test???
   if ( nvParamOffsetsSize != NULL && paramEntry->flags & CGPF_REFERENCED )
   {
      // non varying params
      if ( paramEntry->flags & CGPV_CONSTANT || paramEntry->flags & CGPV_UNIFORM )
      {
         // non sampler types
         if ( !isSampler(( CGtype )paramResource->type ) )
         {
            unsigned short* resPtr = paramResourceTable + paramResource->resource;

            if ( elementResourceIndex >= 0 )
            {
               //advance resPtr to the arrayIndex
               int skipCount = elementResourceIndex;
               while ( skipCount )
               {
                  resPtr++;
                  int embeddedCount = *( resPtr++ );
                  resPtr += embeddedCount;
                  // if an array of matrices, must skip row parameter resources too
                  for ( int row = 1; row <= ( int )rowCount; row++ )
                  {
                     resPtr++;
                     int embeddedCount = *( resPtr++ );
                     resPtr += embeddedCount;
                  }
                  skipCount--;
               }
               //resPtr points to the resource of the arrayIndex
            }

            if ( !isMatrix(( CGtype )( paramResource->type ) ) )
            {
               // advance past register to second field that contains embedded count
               resPtr++;
               unsigned int embeddedConstantCount = *( resPtr++ );
               *nvParamOffsetsSize += sizeof( CgBinaryEmbeddedConstant ) + sizeof( unsigned int ) * ( embeddedConstantCount - 1 );
            }
            else
            {
               for ( int row = 1; row <= ( int )rowCount; row++ )
               {
                  // advance past register to second field that contains embedded count
                  resPtr++;
                  unsigned int embeddedConstantCount = *( resPtr++ );
                  *nvParamOffsetsSize += sizeof( CgBinaryEmbeddedConstant ) + sizeof( unsigned int ) * ( embeddedConstantCount - 1 );
               }
            }

         }
      }

   }


   // entry cursor
   *nextEntry = paramEntry + 1;
}

void AccumulateSizeForParamArray( CgParameterEntry* paramEntry, CgParameterEntry* paramEntries, unsigned int paramEntriesCount,
      unsigned short* paramResourceTable, ELF_section_t* strtab, CgParameterEntry** nextEntry,
      unsigned int prefixLength, int elementResourceIndex,
      size_t* nvParamSize, size_t* nvParamStringsSize, size_t* nvParamOffsetsSize,
      CgParameterSemantic* semanticIndexTable, unsigned int semanticIndexCount,
      CgParameterDefaultValue* defaultIndexTable, unsigned int defaultIndexCount, size_t* nvParamDefaultsSize,
      ELF_section_t* consttab )
{
   // type index is actually the offset into the block of memory following the entries
   CgParameterArray* paramArray = ( CgParameterArray* )(( char* )( paramEntries + paramEntriesCount ) + paramEntry->typeIndex );

   // this parameter's entry says it's an array
   // this parameter's entry says its packing in the flags
   // the next entry specifies its type...
   // ... either a list of every element in the array
   // ... or a single element in the array to represent all because they are the same
   // ... or a single element to specify the layout of the parameter, but not every element has a resource

   // this would give the number of intrinsics OR structs, but if base type is struct
   // to get the number of parameters, must recurse over struct fields
   unsigned int numElements = 1;
   for ( unsigned int dim = 0; dim < paramArray->dimensionCount; dim++ )
   {
      numElements *= paramArray->dimensions[dim];
   };

   unsigned int prefixLengthIn = prefixLength;

   // next entry will always be either an intrinsic or a struct

   if ( !( paramEntry->flags & CGP_UNROLLED ) ) // one bit is not enough cause there are 3 representations!
   {
      // all elements are the same so have one entry to represent all of them
      CgParameterEntry* elementEntry = paramEntry + 1;

      // entries are same type, and packed, so just repeat same param
      for ( unsigned int element = 0; element < numElements; element++ )
      {
         // name length for prefixIn +  rest of array name for array + 16 bytes generously for [index]
         prefixLength = prefixLengthIn + strlen( strtab->data + paramEntry->nameOffset ) + 16;

         // right now i think this is always intrinsics for the not unrolled case, current limitation
         AccumulateSizeForParamResource( elementEntry, paramEntries, paramEntriesCount,
               paramResourceTable, strtab, nextEntry,
               prefixLength, element,
               nvParamSize, nvParamStringsSize, nvParamOffsetsSize,
               semanticIndexTable, semanticIndexCount,
               defaultIndexTable, defaultIndexCount, nvParamDefaultsSize,
               consttab );

         *nextEntry = *nextEntry - 1; // undo the increment on the next entry for this special case of shared entries
      }
      *nextEntry = *nextEntry + 1; // redo the increment upon exiting the loop, could be smarter, whatever
   }
   else if ( paramEntry->flags & CGP_UNROLLED )
   {
      // here everything is unrolled

      CgParameterEntry* elementEntry = paramEntry + 1;

      for ( unsigned int element = 0; element < numElements; element++ )
      {
         // name length for prefixIn +  rest of array name for array + 16 bytes generously for [index]
         prefixLength = prefixLengthIn + strlen( strtab->data + paramEntry->nameOffset ) + 16;

         AccumulateSizeForParams( elementEntry, paramEntries, paramEntriesCount,
               paramResourceTable, strtab, nextEntry,
               prefixLength, -1,
               nvParamSize, nvParamStringsSize, nvParamOffsetsSize,
               semanticIndexTable, semanticIndexCount,
               defaultIndexTable, defaultIndexCount, nvParamDefaultsSize,
               consttab );

         elementEntry = *nextEntry;
      }
   }
}

void AccumulateSizeForParamStruct( CgParameterEntry* paramEntry, CgParameterEntry* paramEntries, unsigned int paramEntriesCount,
      unsigned short* paramResourceTable, ELF_section_t* strtab, CgParameterEntry** nextEntry,
      unsigned int prefixLength, int elementResourceIndex,
      size_t* nvParamSize, size_t* nvParamStringsSize, size_t* nvParamOffsetsSize,
      CgParameterSemantic* semanticIndexTable, unsigned int semanticIndexCount,
      CgParameterDefaultValue* defaultIndexTable, unsigned int defaultIndexCount, size_t* nvParamDefaultsSize,
      ELF_section_t* consttab )
{
   // type index is actually the offset into the block of memory following the entries
   CgParameterStructure* paramStruct = ( CgParameterStructure* )(( char* )( paramEntries + paramEntriesCount ) + paramEntry->typeIndex );

   // add prefix of struct name and .
   // !!! but what about if we are inside an array???  then no struct name
   if ( paramEntry->nameOffset > 0 ) // is this the right test???
   {
      prefixLength += strlen( strtab->data + paramEntry->nameOffset );
   }
   prefixLength += 1; // allow for the dot

   unsigned short memberCount = paramStruct->memberCount;
   CgParameterEntry* memberEntry = paramEntry + 1;

   for ( unsigned int member = 0; member < memberCount; member++ )
   {
      AccumulateSizeForParams( memberEntry, paramEntries, paramEntriesCount,
            paramResourceTable, strtab, nextEntry,
            prefixLength, -1,
            nvParamSize, nvParamStringsSize, nvParamOffsetsSize,
            semanticIndexTable, semanticIndexCount,
            defaultIndexTable, defaultIndexCount, nvParamDefaultsSize,
            consttab );

      memberEntry = *nextEntry;
   }
}

void AccumulateSizeForParams( CgParameterEntry* paramEntry, CgParameterEntry* paramEntries, unsigned int paramEntriesCount,
      unsigned short* paramResourceTable, ELF_section_t* strtab, CgParameterEntry** nextEntry,
      unsigned int prefixLength, int elementResourceIndex,
      size_t* nvParamSize, size_t* nvParamStringsSize, size_t* nvParamOffsetsSize,
      CgParameterSemantic* semanticIndexTable, unsigned int semanticIndexCount,
      CgParameterDefaultValue* defaultIndexTable, unsigned int defaultIndexCount, size_t* nvParamDefaultsSize,
      ELF_section_t* consttab )
{
   // expand for intrinsics, arrays, and structs
   // flags hold which type of parameter it is (along with other information)
   switch ( paramEntry->flags & CGP_TYPE_MASK )
   {
      case CGP_INTRINSIC:
         {
            //printf("resource --- %p\n", paramEntry);
            AccumulateSizeForParamResource( paramEntry, paramEntries, paramEntriesCount,
                  paramResourceTable, strtab, nextEntry,
                  prefixLength, elementResourceIndex,
                  nvParamSize, nvParamStringsSize, nvParamOffsetsSize,
                  semanticIndexTable, semanticIndexCount,
                  defaultIndexTable, defaultIndexCount, nvParamDefaultsSize,
                  consttab );
            break;
         }
      case CGP_ARRAY:
         {
            //printf("array --- %p\n", paramEntry);
            AccumulateSizeForParamArray( paramEntry, paramEntries, paramEntriesCount,
                  paramResourceTable, strtab, nextEntry,
                  prefixLength, elementResourceIndex,
                  nvParamSize, nvParamStringsSize, nvParamOffsetsSize,
                  semanticIndexTable, semanticIndexCount,
                  defaultIndexTable, defaultIndexCount, nvParamDefaultsSize,
                  consttab );
            break;
         }
      case CGP_STRUCTURE:
         {
            //printf("structure --- %p\n", paramEntry);
            AccumulateSizeForParamStruct( paramEntry, paramEntries, paramEntriesCount,
                  paramResourceTable, strtab, nextEntry,
                  prefixLength, elementResourceIndex,
                  nvParamSize, nvParamStringsSize, nvParamOffsetsSize,
                  semanticIndexTable, semanticIndexCount,
                  defaultIndexTable, defaultIndexCount, nvParamDefaultsSize,
                  consttab );
            break;
         }
      default:
         {
            break;
         }

   }

}

// forward declaration for recursion
void PopulateDataForParams( CgParameterEntry* paramEntry, CgParameterEntry* paramEntries, unsigned int paramEntriesCount,
      unsigned short* paramResourceTable, ELF_section_t* strtab, CgParameterEntry** nextEntry,
      int elementResourceIndex, unsigned int* localParamNo, char* nvBinary,
      CgBinaryParameter** nvParams, char** nvParamStrings,
      CgBinaryEmbeddedConstant** nvParamOffsets, float** nvParamDefaults,
      char* prefix, unsigned int prefixLength,
      CgParameterSemantic* semanticIndexTable, unsigned int semanticIndexCount,
      CgParameterDefaultValue* defaultIndexTable, unsigned int defaultIndexCount,
      ELF_section_t* consttab );

void PopulateDataForParamResource( CgParameterEntry* paramEntry, CgParameterEntry* paramEntries, unsigned int paramEntriesCount,
      unsigned short* paramResourceTable, ELF_section_t* strtab, CgParameterEntry** nextEntry,
      int elementResourceIndex, unsigned int* localParamNo, char* nvBinary,
      CgBinaryParameter** nvParams, char** nvParamStrings,
      CgBinaryEmbeddedConstant** nvParamOffsets, float** nvParamDefaults,
      char* prefix, unsigned int prefixLength,
      CgParameterSemantic* semanticIndexTable, unsigned int semanticIndexCount,
      CgParameterDefaultValue* defaultIndexTable, unsigned int defaultIndexCount,
      ELF_section_t* consttab )
{
   // type index is actually the offset into the block of memory following the entries
   CgParameterResource* paramResource = ( CgParameterResource* )(( char* )( paramEntries + paramEntriesCount ) + paramEntry->typeIndex );

   unsigned int rowCount = getMatrixRowCount(( CGtype )paramResource->type );

   // type
   ( *nvParams )->type = ( CGtype )( paramResource->type );
   // sub type for row params of matrix
   for ( int row = 1; row <= ( int )rowCount; row++ )
      ( *nvParams + row )->type = getMatrixRowType(( CGtype )paramResource->type );

   // varying parameters
   if (( paramEntry->flags & CGPV_MASK ) == CGPV_VARYING )
   {
      ( *nvParams )->res = ( CGresource )paramResource->resource;
      ( *nvParams )->resIndex = -1;
   }
   else if ((( paramEntry->flags & CGPV_MASK ) & CGPV_CONSTANT ) || (( paramEntry->flags & CGPV_MASK ) & CGPV_UNIFORM ) )
   {
      // sampler types
      if ( isSampler(( CGtype )paramResource->type ) )
      {
         ( *nvParams )->res = ( CGresource )paramResource->resource; //should be CG_TEXUNIT0 .. 15
         ( *nvParams )->resIndex = 0;
      }
      // nonsampler types
      else
      {
         // CG_B requires support too
         for ( int row = 0; row <= ( int )rowCount; row++ )
         {
            if ( paramResource->type == CGP_SCF_BOOL ) //(paramEntry->flags & CGP_SCFBOOL)
            {
               ( *nvParams + row )->res = CG_B;
               ( *nvParams )->type = CG_BOOL;
            }
            else
            {
               ( *nvParams + row )->res = CG_C;
            }
         }

         // vertex programs
         if ( nvParamOffsets == NULL )
         {
            // element of an array
            if ( elementResourceIndex >= 0 )
            {
               if ( !isMatrix(( CGtype )( paramResource->type ) ) )
               {
                  unsigned short tempResIndex = *( paramResourceTable + paramResource->resource + elementResourceIndex );
                  ( *nvParams )->resIndex = ( tempResIndex == 0xFFFF ) ? -1 : ( int )tempResIndex;
               }
               else if ( paramEntry->flags & CGP_CONTIGUOUS )
               {
                  unsigned short tempResIndex = *( paramResourceTable + paramResource->resource + elementResourceIndex );
                  ( *nvParams )->resIndex = ( tempResIndex == 0xFFFF ) ? -1 : ( int )tempResIndex;
                  for ( int row = 1; row <= ( int )rowCount; row++ )
                  {
                     unsigned short tempResIndex = *( paramResourceTable + paramResource->resource + elementResourceIndex ) + row - 1;
                     ( *nvParams + row )->resIndex = ( tempResIndex == 0xFFFF ) ? -1 : ( int )tempResIndex;
                  }
               }
               else
               {
                  unsigned short tempResIndex = *( paramResourceTable + paramResource->resource + elementResourceIndex * rowCount );
                  ( *nvParams )->resIndex = ( tempResIndex == 0xFFFF ) ? -1 : ( int )tempResIndex;
                  for ( int row = 1; row <= ( int )rowCount; row++ )
                  {
                     unsigned short tempResIndex = *( paramResourceTable + paramResource->resource + elementResourceIndex * rowCount + row - 1 );
                     ( *nvParams + row )->resIndex = ( tempResIndex == 0xFFFF ) ? -1 : ( int )tempResIndex;
                  }
               }
            }
            // not array
            else
            {
               if ( !isMatrix(( CGtype )( paramResource->type ) ) )
               {
                  unsigned short tempResIndex = paramResource->resource;
                  ( *nvParams )->resIndex = ( tempResIndex == 0xFFFF ) ? -1 : ( int )tempResIndex;
               }
               else
               {
                  if ( paramEntry->flags & CGP_CONTIGUOUS )
                  {
                     unsigned short tempResIndex = paramResource->resource;
                     ( *nvParams )->resIndex = ( tempResIndex == 0xFFFF ) ? -1 : ( int )tempResIndex;
                     for ( int row = 1; row <= ( int )rowCount; row++ )
                     {
                        unsigned short tempResIndex = paramResource->resource + row - 1;
                        ( *nvParams + row )->resIndex = ( tempResIndex == 0xFFFF ) ? -1 : ( int )tempResIndex;
                     }
                  }
                  else
                  {
                     unsigned short tempResIndex = *( paramResourceTable + paramResource->resource );
                     ( *nvParams )->resIndex = ( tempResIndex == 0xFFFF ) ? -1 : ( int )tempResIndex;
                     for ( int row = 1; row <= ( int )rowCount; row++ )
                     {
                        unsigned short tempResIndex = *( paramResourceTable + paramResource->resource + row - 1 );
                        ( *nvParams + row )->resIndex = ( tempResIndex == 0xFFFF ) ? -1 : ( int )tempResIndex;
                     }
                  }
               }
            }
         }
         // fragment programs
         else
         {
            unsigned short* resPtr = paramResourceTable + paramResource->resource;

            if ( elementResourceIndex >= 0 )
            {
               // !!! matrices should be handled here too, i think
               //advance resPtr to the arrayIndex
               int skipCount = elementResourceIndex;
               while ( skipCount )
               {
                  resPtr++;
                  int embeddedCount = *( resPtr++ );
                  resPtr += embeddedCount;
                  // if an array of matrices, must skip row parameter resources too
                  for ( int row = 1; row <= ( int )rowCount; row++ )
                  {
                     resPtr++;
                     int embeddedCount = *( resPtr++ );
                     resPtr += embeddedCount;
                  }
                  skipCount--;
               }
               //resPtr points to the resource of the arrayIndex
            }

            if ( !isMatrix(( CGtype )( paramResource->type ) ) )
            {
               unsigned short tempResIndex = *( resPtr++ );
               ( *nvParams )->resIndex = ( tempResIndex == 0xFFFF ) ? -1 : ( int )tempResIndex;
               unsigned int embeddedConstantCount = *( resPtr++ );
               ( *nvParamOffsets )->ucodeCount = embeddedConstantCount;
               unsigned int ec = 0;
               for ( ec = 0; ec < embeddedConstantCount; ec++ )
                  ( *nvParamOffsets )->ucodeOffset[ec] = *( resPtr++ );
               // set the field in the nv param
               ( *nvParams )->embeddedConst = ( char* )( *nvParamOffsets ) - nvBinary;
               // forward pointer to nvParamOffsets
               *nvParamOffsets = ( CgBinaryEmbeddedConstant* )( &(( *nvParamOffsets )->ucodeOffset[ec] ) );
            }
            else
            {
               for ( int row = 1; row <= ( int )rowCount; row++ )
               {
                  unsigned short tempResIndex = *( resPtr++ );
                  ( *nvParams + row )->resIndex = ( tempResIndex == 0xFFFF ) ? -1 : ( int )tempResIndex;
                  unsigned int embeddedConstantCount = *( resPtr++ );
                  ( *nvParamOffsets )->ucodeCount = embeddedConstantCount;
                  unsigned int ec = 0;
                  for ( ec = 0; ec < embeddedConstantCount; ec++ )
                     ( *nvParamOffsets )->ucodeOffset[ec] = *( resPtr++ );
                  // set the field in the nv param
                  ( *nvParams + row )->embeddedConst = ( char* )( *nvParamOffsets ) - nvBinary;
                  // forward pointer to nvParamOffsets
                  *nvParamOffsets = ( CgBinaryEmbeddedConstant* )( &(( *nvParamOffsets )->ucodeOffset[ec] ) );
               }
            }
         }
      }

   }
   // RESOURCE DECODING ALGORTITHM END


   // name
   if ( paramEntry->nameOffset )
   {
      // copy the name and store its offset in the param
      // i think prefix is already handled for this base case
      strcpy( *nvParamStrings, strtab->data + paramEntry->nameOffset );
   }
   // set the name offset in the param, starting from the prefix
   char* namebase = NULL;
   if ( prefix )
   {
      ( *nvParams )->name = prefix - nvBinary;
      namebase = prefix;
   }
   else
   {
      ( *nvParams )->name = *nvParamStrings - nvBinary;
      namebase = *nvParamStrings;
   }


   // increment the names cursor
   *nvParamStrings += strlen( *nvParamStrings ) + 1;

   // and now handle the matrix rows
   for ( int row = 1; row <= ( int )rowCount; row++ )
   {
      // copy the name and store its offset in the param
      sprintf( *nvParamStrings, "%s[%d]", namebase, row - 1 );
      // set the name offset in the param
      ( *nvParams + row )->name = *nvParamStrings - nvBinary;
      // increment the names cursor
      *nvParamStrings += strlen( *nvParamStrings ) + 1;
   }

   // totally inefficient, but easiest implementation for now is to do the On^2 search
   // in the semantic and default tables for the data associated with this parameter
   unsigned int entryIndex = paramEntry - paramEntries;

   // semantic
   char* semantic = NULL;
   for ( unsigned int sem = 0; sem < semanticIndexCount; sem++ )
   {
      CgParameterSemantic* semanticIndex = semanticIndexTable + sem;
      if ( semanticIndex->entryIndex == entryIndex )
      {
         // found the semantic for this parameter
         semantic = strtab->data + semanticIndex->semanticOffset;
         break;
      }
   }
   if ( semantic )
   {
      // copy the name and store its offset in the param
      strcpy( *nvParamStrings, semantic );
      // set the name offset in the param (or params if a matrix)
      for ( int row = 0; row <= ( int )rowCount; row++ )
         ( *nvParams )->semantic = *nvParamStrings - nvBinary;
      // increment the names cursor
      *nvParamStrings += strlen( semantic ) + 1;
   }

   // default
   float* defaults = NULL;
   for ( unsigned int def = 0; def < defaultIndexCount; def++ )
   {
      CgParameterDefaultValue* defaultIndex = defaultIndexTable + def;
      if ( defaultIndex->entryIndex == entryIndex ) // found the default for this parameter
         defaults = ( float* )consttab->data + defaultIndex->defaultValueIndex;
   }
   if ( defaults )
   {
      // defaults are always padded to 4 floats per param
      unsigned int defaultsCount = ( rowCount ? rowCount : 1 ) * 4;
      // copy the values into the right place in the buffer
      memcpy( *nvParamDefaults, defaults, defaultsCount * sizeof( float ) );
      // set the field in the param to point to them
      if ( rowCount == 0 )
         ( *nvParams )->defaultValue = ( char* )( *nvParamDefaults ) - nvBinary;
      else
      {
         for ( int row = 1; row <= ( int )rowCount; row++ )
            ( *nvParams + row )->defaultValue = ( char* )( *nvParamDefaults + 4 * ( row - 1 ) ) - nvBinary;
      }

      // forward the buffer pointer
      *nvParamDefaults += defaultsCount;
   }


   // fill other fields, once for the matrix and once for every row
   for ( int row = 0; row <= ( int )rowCount; row++ )
   {
      // var
      unsigned int variability = paramEntry->flags & CGPV_MASK;
      if ( variability == CGPV_VARYING )
         ( *nvParams )->var = CG_VARYING;
      else if ( variability == CGPV_UNIFORM )
         ( *nvParams )->var = CG_UNIFORM;
      else if ( variability == CGPV_CONSTANT )
         ( *nvParams )->var = CG_CONSTANT;
      else if ( variability == CGPV_MIXED )
         ( *nvParams )->var = CG_MIXED;
      else
         ( *nvParams )->var = ( CGenum )0;

      //direction
      unsigned int direction = paramEntry->flags & CGPD_MASK;
      if ( direction == CGPD_IN )
         ( *nvParams )->direction = CG_IN;
      else if ( direction == CGPD_OUT )
         ( *nvParams )->direction = CG_OUT;
      else if ( direction == CGPD_INOUT )
         ( *nvParams )->direction = CG_INOUT;
      else
         ( *nvParams )->direction = ( CGenum )0;

      // paramno
      if ( paramEntry->flags & CGPF_GLOBAL )
         ( *nvParams )->paramno = -1;
      else if ( paramEntry->flags & CGP_INTERNAL )
         ( *nvParams )->paramno = -2;
      else
      {
         ( *nvParams )->paramno = *localParamNo;
         if ( row == 0 )
            *localParamNo += 1;
      }

      // isReferenced
      ( *nvParams )->isReferenced = (( paramEntry->flags & CGPF_REFERENCED ) != 0 );

      // isShared
      ( *nvParams )->isShared = (( paramEntry->flags & CGPF_SHARED ) != 0 );


      // increment param cursor
      *nvParams += 1;


   }

   // entry cursor
   *nextEntry = paramEntry + 1;

}

void PopulateDataForParamArray( CgParameterEntry* paramEntry, CgParameterEntry* paramEntries, unsigned int paramEntriesCount,
      unsigned short* paramResourceTable, ELF_section_t* strtab, CgParameterEntry** nextEntry,
      int elementResourceIndex, unsigned int* localParamNo, char* nvBinary,
      CgBinaryParameter** nvParams, char** nvParamStrings,
      CgBinaryEmbeddedConstant** nvParamOffsets, float** nvParamDefaults,
      char* prefix, unsigned int prefixLength,
      CgParameterSemantic* semanticIndexTable, unsigned int semanticIndexCount,
      CgParameterDefaultValue* defaultIndexTable, unsigned int defaultIndexCount,
      ELF_section_t* consttab )
{
   // type index is actually the offset into the block of memory following the entries
   CgParameterArray* paramArray = ( CgParameterArray* )(( char* )( paramEntries + paramEntriesCount ) + paramEntry->typeIndex );

   // this would give the number of intrinsics OR structs, but if base type is struct
   // to get the number of parameters, must recurse over struct fields
   unsigned int numElements = 1;
   for ( unsigned int dim = 0; dim < paramArray->dimensionCount; dim++ )
      numElements *= paramArray->dimensions[dim];

   // for name handling
   char* prefixIn = prefix;
   unsigned int prefixLengthIn = prefixLength;

   if ( !( paramEntry->flags & CGP_UNROLLED ) ) // one bit is not enough cause there are 3 representations!
   {
      // all elements are the same so have one entry to represent all of them
      CgParameterEntry* elementEntry = paramEntry + 1;

      // entries are same type, and packed, so just repeat same param
      for ( unsigned int element = 0; element < numElements; element++ )
      {
         // where is the array name???  in paramEntry or elementEntry???

         // name
         // copy prefix if it's not the first element whose prefix is already in place
         if ( element > 0 )
         {
            strncpy( *nvParamStrings, prefixIn, prefixLengthIn );
            prefix = *nvParamStrings;
            *nvParamStrings += prefixLengthIn;
         }
         // rest of array name for array and index
         sprintf( *nvParamStrings, "%s[%d]", ( strtab->data + paramEntry->nameOffset ), element );

         if ( prefix == NULL )
            prefix = *nvParamStrings;

         *nvParamStrings += strlen( *nvParamStrings );

         prefixLength = strlen( prefix );

         PopulateDataForParamResource( elementEntry, paramEntries, paramEntriesCount,
               paramResourceTable, strtab, nextEntry,
               element, localParamNo, nvBinary,
               nvParams, nvParamStrings, nvParamOffsets, nvParamDefaults,
               prefix, prefixLength,
               semanticIndexTable, semanticIndexCount,
               defaultIndexTable, defaultIndexCount,
               consttab );

         *nextEntry = *nextEntry - 1; // undo the increment on the next entry for this special case of shared entries
      }
      *nextEntry = *nextEntry + 1; // redo the increment on the next entry upon leaving the loop
   }
   else if ( paramEntry->flags & CGP_UNROLLED )
   {
      // here everything is unrolled

      CgParameterEntry* elementEntry = paramEntry + 1;

      for ( unsigned int element = 0; element < numElements; element++ )
      {
         // where is the array name???  in paramEntry or elementEntry???

         // name
         // copy prefix if it's not the first element whose prefix is already in place
         if ( element > 0 )
         {
            strncpy( *nvParamStrings, prefixIn, prefixLengthIn );
            prefix = *nvParamStrings;
            *nvParamStrings += prefixLengthIn;
         }
         // rest of array name for array and index
         sprintf( *nvParamStrings, "%s[%d]", ( strtab->data + paramEntry->nameOffset ), element );
         *nvParamStrings += strlen( *nvParamStrings );
         prefixLength = strlen( prefix );

         PopulateDataForParams( elementEntry, paramEntries, paramEntriesCount,
               paramResourceTable, strtab, nextEntry,
               -1, localParamNo, nvBinary,
               nvParams, nvParamStrings, nvParamOffsets, nvParamDefaults,
               prefix, prefixLength,
               semanticIndexTable, semanticIndexCount,
               defaultIndexTable, defaultIndexCount,
               consttab );

         elementEntry = *nextEntry;
      }
   }
}

void PopulateDataForParamStruct( CgParameterEntry* paramEntry, CgParameterEntry* paramEntries, unsigned int paramEntriesCount,
      unsigned short* paramResourceTable, ELF_section_t* strtab, CgParameterEntry** nextEntry,
      int elementResourceIndex, unsigned int* localParamNo, char* nvBinary,
      CgBinaryParameter** nvParams, char** nvParamStrings,
      CgBinaryEmbeddedConstant** nvParamOffsets, float** nvParamDefaults,
      char* prefix, unsigned int prefixLength,
      CgParameterSemantic* semanticIndexTable, unsigned int semanticIndexCount,
      CgParameterDefaultValue* defaultIndexTable, unsigned int defaultIndexCount,
      ELF_section_t* consttab )
{
   // type index is actually the offset into the block of memory following the entries
   CgParameterStructure* paramStruct = ( CgParameterStructure* )(( char* )( paramEntries + paramEntriesCount ) + paramEntry->typeIndex );

   unsigned short memberCount = paramStruct->memberCount;
   CgParameterEntry* memberEntry = paramEntry + 1;

   // set the prefix pointer if it is not already set
   if ( prefix == NULL )
      prefix = *nvParamStrings;

   // add prefix of struct name and .
   // !!! but what about if we are inside an array???  then no struct name?  or is . built in?
   if ( paramEntry->nameOffset > 0 ) // is this the right test???
   {
      // name
      unsigned int nameLength = strlen( strtab->data + paramEntry->nameOffset );
      // copy the name and store its offset in the param
      strcpy( *nvParamStrings, strtab->data + paramEntry->nameOffset );

      // increment the names cursor but don't include null term
      *nvParamStrings += nameLength;
      prefixLength += nameLength;
   }

   // add the dot
   strcpy( *nvParamStrings, "." );
   *nvParamStrings += 1;
   prefixLength += 1;

   for ( unsigned int member = 0; member < memberCount; member++ )
   {
      // put the prefix in place for each subsequent member
      if ( member > 0 )
      {
         strncpy( *nvParamStrings, prefix, prefixLength );
         prefix = *nvParamStrings;
         *nvParamStrings += prefixLength;
      }

      PopulateDataForParams( memberEntry, paramEntries, paramEntriesCount,
            paramResourceTable, strtab, nextEntry,
            -1, localParamNo, nvBinary,
            nvParams, nvParamStrings, nvParamOffsets, nvParamDefaults,
            prefix, prefixLength,
            semanticIndexTable, semanticIndexCount,
            defaultIndexTable, defaultIndexCount,
            consttab );

      memberEntry = *nextEntry;
   }
}

void PopulateDataForParams( CgParameterEntry* paramEntry, CgParameterEntry* paramEntries, unsigned int paramEntriesCount,
      unsigned short* paramResourceTable, ELF_section_t* strtab, CgParameterEntry** nextEntry,
      int elementResourceIndex, unsigned int* localParamNo, char* nvBinary,
      CgBinaryParameter** nvParams, char** nvParamStrings,
      CgBinaryEmbeddedConstant** nvParamOffsets, float** nvParamDefaults,
      char* prefix, unsigned int prefixLength,
      CgParameterSemantic* semanticIndexTable, unsigned int semanticIndexCount,
      CgParameterDefaultValue* defaultIndexTable, unsigned int defaultIndexCount,
      ELF_section_t* consttab )
{

   // expand for intrinsics, arrays, and structs
   // flags hold which type of parameter it is (along with other information)
   switch ( paramEntry->flags & CGP_TYPE_MASK )
   {
      case CGP_INTRINSIC:
         {
            //printf("resource --- %p\n", paramEntry);
            PopulateDataForParamResource( paramEntry, paramEntries, paramEntriesCount,
                  paramResourceTable, strtab, nextEntry,
                  elementResourceIndex, localParamNo, nvBinary,
                  nvParams, nvParamStrings, nvParamOffsets, nvParamDefaults,
                  prefix, prefixLength,
                  semanticIndexTable, semanticIndexCount,
                  defaultIndexTable, defaultIndexCount,
                  consttab );
            break;
         }
      case CGP_ARRAY:
         {
            //printf("array --- %p\n", paramEntry);
            PopulateDataForParamArray( paramEntry, paramEntries, paramEntriesCount,
                  paramResourceTable, strtab, nextEntry,
                  elementResourceIndex, localParamNo, nvBinary,
                  nvParams, nvParamStrings, nvParamOffsets, nvParamDefaults,
                  prefix, prefixLength,
                  semanticIndexTable, semanticIndexCount,
                  defaultIndexTable, defaultIndexCount,
                  consttab );
            break;
         }
      case CGP_STRUCTURE:
         {
            //printf("structure --- %p\n", paramEntry);
            PopulateDataForParamStruct( paramEntry, paramEntries, paramEntriesCount,
                  paramResourceTable, strtab, nextEntry,
                  elementResourceIndex, localParamNo, nvBinary,
                  nvParams, nvParamStrings, nvParamOffsets, nvParamDefaults,
                  prefix, prefixLength,
                  semanticIndexTable, semanticIndexCount,
                  defaultIndexTable, defaultIndexCount,
                  consttab );
            break;
         }
      default:
         {
            break;
         }

   }
}

bool rglCgCreateProgramChecks( CGcontext ctx, CGprofile profile, CGenum program_type )
{
   // check context
   if ( !CG_IS_CONTEXT( ctx ) )
   {
      rglCgRaiseError( CG_INVALID_CONTEXT_HANDLE_ERROR );
      return false;
   }

   // check the profile.
   switch ( profile )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
      case CG_PROFILE_SCE_FP_TYPEB:
      case CG_PROFILE_SCE_VP_RSX:
      case CG_PROFILE_SCE_FP_RSX:
         break;
      default:
         rglCgRaiseError( CG_UNKNOWN_PROFILE_ERROR );
         return false;
   }

   // check the program_type
   switch ( program_type )
   {
      case CG_BINARY:
      case CG_SOURCE:
         break;
      default: // currently reject CG_OBJECT (assembly code in ASCII) and CG_UNDEFINED
         rglCgRaiseError( CG_INVALID_ENUMERANT_ERROR );
         return false;
   }

   return true;
}

bool cgOpenElf( const void *ptr, size_t size, CGELFBinary *elfBinary )
{
   while ( 1 )
   {
      size_t symbolSize;
      size_t symbolCount;
      const char *symbolstrtab;
      const char *symtab = findSymbolSectionInPlace((const char *)ptr, &symbolSize, &symbolCount, &symbolstrtab );
      if ( !symtab )
         break;

      size_t shadertabSize;
      const char *shadertab = findSectionInPlace((const char*)ptr, ".shadertab", &shadertabSize );
      if ( !shadertab )
         break;
      size_t strtabSize;
      const char *strtab = findSectionInPlace((const char*)ptr, ".strtab", &strtabSize );
      if ( !strtab )
         break;
      size_t consttabSize;
      const char *consttab = findSectionInPlace((const char* )ptr, ".const", &consttabSize );
      if ( !consttab )
         break;

      elfBinary->elfFile = ( const char* )ptr;
      elfBinary->elfFileSize = size;
      elfBinary->symtab = symtab;
      elfBinary->symbolSize = symbolSize;
      elfBinary->symbolCount = symbolCount;
      elfBinary->symbolstrtab = symbolstrtab;

      elfBinary->shadertab = shadertab;
      elfBinary->shadertabSize = shadertabSize;
      elfBinary->strtab = strtab;
      elfBinary->strtabSize = strtabSize;

      elfBinary->consttab = consttab;
      elfBinary->consttabSize = consttabSize;

      return true;
   }

   return false;
}

bool cgGetElfProgramByIndex( CGELFBinary *elfBinary, int index, CGELFProgram *elfProgram )
{
   while ( true )
   {
      char sectionName[64];
      sprintf( sectionName, ".text%04i", index );
      size_t texttabSize;
      const char *texttab = findSectionInPlace( elfBinary->elfFile, sectionName, &texttabSize );
      if ( !texttab )
         break;
      sprintf( sectionName, ".paramtab%04i", index );
      size_t paramtabSize;
      const char *paramtab = findSectionInPlace( elfBinary->elfFile, sectionName, &paramtabSize );
      if ( !paramtab )
         break;

      elfProgram->texttab = texttab;
      elfProgram->texttabSize = texttabSize;
      elfProgram->paramtab = paramtab;
      elfProgram->paramtabSize = paramtabSize;
      elfProgram->index = index;
      return true;
   }
   return false;
}

static bool cgGetElfProgramByName( CGELFBinary *elfBinary, const char *name, CGELFProgram *elfProgram )
{
   //if no name try to return the first program
   int res;
   if ( name == NULL || name[0] == '\0' )
      res = 0;
   else
      res = lookupSymbolValueInPlace( elfBinary->symtab, elfBinary->symbolSize, elfBinary->symbolCount, elfBinary->symbolstrtab, name );

   if ( res != -1 )
      return cgGetElfProgramByIndex( elfBinary, res, elfProgram );
   else
      return false;
}

CGprogram rglCgCreateProgram( CGcontext ctx, CGprofile profile, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader, const char *stringTable, const float *defaultValues )
{
   // Create the program structure.
   // all the structural data is filled in here,
   // as well as the profile.
   // The parameters and the actual program are generated from the ABI specific calls.

   _CGprogram* prog = ( _CGprogram* )malloc( sizeof( _CGprogram ) );
   if ( NULL == prog )
   {
      rglCgRaiseError( CG_MEMORY_ALLOC_ERROR );
      return NULL;
   }

   // zero out the fields
   rglCgProgramZero( prog );

   // fill in the fields we know
   prog->parentContext = _cgGetContextPtr( ctx );
   prog->header.profile = profile;

   int success = 0;

   // create a name for the program and record it in the object
   CGprogram id = ( CGprogram )rglCreateName( &_CurrentContext->cgProgramNameSpace, prog );
   if ( !id )
   {
      free( prog );
      rglCgRaiseError( CG_MEMORY_ALLOC_ERROR );
      return NULL;
   }
   prog->id = id;

   //hack to counter removal of TypeC during beta
   if ( profile == ( CGprofile )7005 )
      profile = CG_PROFILE_SCE_VP_RSX;
   if ( profile == ( CGprofile )7006 )
      profile = CG_PROFILE_SCE_FP_RSX;

   // load the binary into the program object
   switch ( profile )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
         //case CG_PROFILE_SCE_VP_TYPEC:
      case CG_PROFILE_SCE_VP_RSX:
         // TODO ************** need to include the entry symbol too
         success = rglpGenerateVertexProgram( prog, programHeader, ucode, parameterHeader, stringTable, defaultValues );
         break;
      case CG_PROFILE_SCE_FP_TYPEB:
         //case CG_PROFILE_SCE_FP_TYPEC:
      case CG_PROFILE_SCE_FP_RSX:
         success = rglpGenerateFragmentProgram( prog, programHeader, ucode, parameterHeader, stringTable, defaultValues );
         break;
      default:
         // should never reach here
         break;
   }

   // if the creation failed, free all resources.
   // the error was raised when the error was encoutered.
   if ( success == 0 )
   {
      // free the program object
      free( prog );
      // release the id too
      rglEraseName( &_CurrentContext->cgProgramNameSpace, (unsigned int)id );
      return NULL;
   }

   // success! add the program to the program list in the context.
   rglCgProgramPushFront( prog->parentContext, prog );
   if ( _cgProgramCreateHook ) _cgProgramCreateHook( prog );

   // everything worked.
   return id;
}

CG_API CGprogram cgCreateProgram( CGcontext ctx,
      CGenum program_type,
      const char* program,
      CGprofile profile,
      const char* entry,
      const char** args )
{
   // Load a program from a memory pointer.
   // NOTE: in our API all programs are pre-compiled binaries
   // so entry point and compiler arguments are ignored.

   //hack to counter removal of TypeC during beta
   if ( profile == ( CGprofile )7005 )
      profile = CG_PROFILE_SCE_VP_RSX;
   if ( profile == ( CGprofile )7006 )
      profile = CG_PROFILE_SCE_FP_RSX;

   //hack to counter change of defines for program_type at r5294
   // previously CG_BINARY was defined the same as CG_ROW_MAJOR
   // if those values are passed in here, move them to the new values and remove this hack after we have
   // an sdk that incorporates these changes so that prebuild libs (aka debugfont) can be used meanwhile
   if ( program_type == CG_ROW_MAJOR )
      program_type = CG_BINARY;

   if ( !rglCgCreateProgramChecks( ctx, profile, program_type ) )
      return NULL;

   //data to extract from the buffer passed:
   CgProgramHeader *programHeader = NULL;
   const void *ucode = NULL;
   CgParameterTableHeader *parameterHeader = NULL;
   const char *stringTable = NULL;
   const float *defaultValues = NULL;

   //first step, compile any source file
   const char *binaryBuffer = NULL;
   char* compiled_program = NULL;
   if ( program_type == CG_SOURCE )
   {
      if ( _cgRTCgcCompileProgramHook )
      {
         _cgRTCgcCompileProgramHook( program, (profile == CG_PROFILE_SCE_FP_RSX) ? "sce_fp_rsx" : "sce_vp_rsx", entry, args, &compiled_program );
         if ( !compiled_program )
         {
            rglCgRaiseError( CG_COMPILER_ERROR );
            return NULL;
         }
         binaryBuffer = compiled_program;
      }
      else
      {
         //_RGL_REPORT_EXTRA( RGL_REPORT_CG_ERROR, "The CG runtime compiler hasn't been setup. cgRTCgcInit() should be called prior to this function." );
         rglCgRaiseError( CG_INVALID_ENUMERANT_ERROR );
         return NULL;
      }
   }
   else
      binaryBuffer = program;

   bool bConvertedToElf = false;

   //At that point we have a binary file which is either any ELF or an NV format file
   const unsigned int ElfTag = 0x7F454C46; // == MAKEFOURCC(0x7F,'E','L','F');
   if ( !( *( unsigned int* )binaryBuffer == ElfTag ) )
   {
      //we have an NV file, convert it to the runtime format

      // if it was initially binary, throw warning about old format and recommend conversion to new with cgnv2elf
      // don't throw the warning if it was source, cause clearly that would have been on purpose.
      if ( program_type == CG_BINARY )
      {
         //_RGL_REPORT_EXTRA( RGL_REPORT_CG_ERROR, "A binary shader is being loaded using a deprecated binary format.  Please use the cgnv2elf tool to convert to the new, memory-saving, faster-loading format." );
      }

      //convert from NV format to the runtime format
      int compiled_program_size = 0;
      std::vector<char> stringTableArray;
      std::vector<float> defaultValuesArray;
      CgBinaryProgram* nvProgram = ( CgBinaryProgram* )binaryBuffer;
      char *runtimeElfShader = NULL;
      //check the endianness
      int totalSize;
      if (( nvProgram->profile != CG_PROFILE_SCE_FP_TYPEB ) && ( nvProgram->profile != CG_PROFILE_SCE_VP_TYPEB ) &&
            ( nvProgram->profile != ( CGprofile )7006 ) && ( nvProgram->profile != ( CGprofile )7005 ) &&
            ( nvProgram->profile != CG_PROFILE_SCE_FP_RSX ) && ( nvProgram->profile != CG_PROFILE_SCE_VP_RSX ) )
      {
         totalSize = endianSwapWord( nvProgram->totalSize );
      }
      else
         totalSize = nvProgram->totalSize;
      int res = convertNvToElfFromMemory( binaryBuffer, totalSize, 2, 0, ( void** ) & runtimeElfShader, &compiled_program_size, stringTableArray, defaultValuesArray );
      if (res != 0)
      {
         //_RGL_REPORT_EXTRA( RGL_REPORT_CG_ERROR, "invalid CG binary program" );
         rglCgRaiseError( CG_COMPILER_ERROR );
         if ( compiled_program )
            _cgRTCgcFreeCompiledProgramHook( compiled_program );
         return NULL;
      }

      if ( compiled_program )
         _cgRTCgcFreeCompiledProgramHook( compiled_program );

      //TODO: remove all the unnecessary data shuffling, when this gets included in the compiler
      //prepare the same buffer as the one that would be extracted from the binary file
      size_t stringTableSize = stringTableArray.size() * sizeof( stringTable[0] );
      size_t defaultTableSize = defaultValuesArray.size() * sizeof( defaultValues[0] );
      int paddedSize = rglPad( compiled_program_size, 4 );
      //the following pointer will be remembered in the _CGprogram structure to be freed when the
      //program will be deleted, the ucode and the param info are used in place at runtime
      char *runtimeElf = ( char* )memalign( 16, paddedSize + stringTableSize + defaultTableSize );
      if ( !runtimeElf )
      {
         rglCgRaiseError( CG_MEMORY_ALLOC_ERROR );
         return NULL;
      }
      bConvertedToElf = true; //to free the memalign
      //program header, ucode and parameters
      memcpy( runtimeElf, runtimeElfShader, compiled_program_size );

      //we can free the intermediate buffer used to receive the converted program //may be we should keep it rather than reallocating ?
      convertNvToElfFreeBinaryShader( runtimeElfShader );

      //default values
      //the pointer is stored right after the program data ( padded to 4 ) and after the string table pointer
      float* pDefaultValues = ( float* )(( char* )runtimeElf + paddedSize );
      defaultValues = pDefaultValues;

      if ( defaultTableSize )
         memcpy( pDefaultValues, &defaultValuesArray[0], defaultTableSize );

      //string table
      //the pointer is stored right after the program data ( padded to 4 )
      char *pStringTable = ( char* )runtimeElf + paddedSize + defaultTableSize;
      stringTable = pStringTable;

      if ( stringTableSize )
         memcpy( pStringTable, &stringTableArray[0], stringTableSize );


      //success
      programHeader = ( CgProgramHeader* )runtimeElf;
      size_t elfUcodeSize = programHeader->instructionCount * 16;
      size_t ucodeOffset = rglPad( sizeof( CgProgramHeader ), 16 );
      size_t parameterOffset = rglPad( ucodeOffset + elfUcodeSize, 16 );
      ucode = ( char* )runtimeElf + ucodeOffset;
      parameterHeader = ( CgParameterTableHeader* )(( char* )runtimeElf + parameterOffset );
   }
   else
   {
      //we have an ELF file, it will be used in place ( case where the elf file has been passed as a memory pointer )
      CGELFBinary elfBinary;
      CGELFProgram elfProgram;
      if ((( intptr_t )binaryBuffer ) & 15 )
      {
         //_RGL_REPORT_EXTRA( RGL_REPORT_CG_ERROR, "CG Binary not aligned on 16 bytes, needed for ucode section" );
         rglCgRaiseError( CG_PROGRAM_LOAD_ERROR );
         return NULL;
      }
      bool res = cgOpenElf( binaryBuffer, 0, &elfBinary );
      if ( !res )
      {
         //_RGL_REPORT_EXTRA( RGL_REPORT_CG_ERROR, "not a valid ELF" );
         rglCgRaiseError( CG_PROGRAM_LOAD_ERROR );
         return NULL;
      }
      if ( !cgGetElfProgramByName( &elfBinary, entry, &elfProgram ) )
      {
         //_RGL_REPORT_EXTRA( RGL_REPORT_CG_ERROR, "couldn't find the shader entry in the CG binary" );
         return NULL;
      }

      //success
      programHeader = ( CgProgramHeader* )elfBinary.shadertab + elfProgram.index;
      ucode = ( char* )elfProgram.texttab;
      parameterHeader = ( CgParameterTableHeader* )elfProgram.paramtab;
      stringTable = elfBinary.strtab;
      defaultValues = ( float* )elfBinary.consttab;
   }

   CGprogram prog = rglCgCreateProgram( ctx, profile, programHeader, ucode, parameterHeader, stringTable, defaultValues );

   //if we used the runtime compiler/converter we need to delete the buffer on exit
   if ( bConvertedToElf )
   {
      _CGprogram* ptr = _cgGetProgPtr( prog );
      ptr->runtimeElf = programHeader;
   }

   return prog;
}

CG_API CGprogram cgCreateProgramFromFile( CGcontext ctx,
      CGenum program_type,
      const char* program_file,
      CGprofile profile,
      const char* entry,
      const char** args )
{
   //hack to counter removal of TypeC during beta
   if ( profile == ( CGprofile )7005 )
      profile = CG_PROFILE_SCE_VP_RSX;
   if ( profile == ( CGprofile )7006 )
      profile = CG_PROFILE_SCE_FP_RSX;

   //hack to counter change of defines for program_type at r5294
   // previously CG_BINARY was defined the same as CG_ROW_MAJOR
   // if those values are passed in here, move them to the new values and remove this hack after we have
   // an sdk that incorporates these changes so that prebuild libs (aka debugfont) can be used meanwhile
   if ( program_type == CG_ROW_MAJOR )
      program_type = CG_BINARY;

   if ( !rglCgCreateProgramChecks( ctx, profile, program_type ) )
      return NULL;

   // load a program from a file
   // NOTE: in our API all programs are pre-compiled binaries
   // so profile and compiler arguments are ignored.
   FILE* fp = NULL;
   if ( RGL_LIKELY( program_type == CG_BINARY ) )
   {
      CGprogram ret = NULL;
      //assume we have an elf at that point
      //if (filetag == ElfTag)

      _CGcontext *context = _cgGetContextPtr( ctx );
      CGprogramGroup group = NULL;

      //can we find it in the groups already loaded ?
      group = context->groupList;
      while ( group )
      {
         //check the group name
         const char *groupName = rglCgGetProgramGroupName( group );
         if ( groupName && !strcmp( groupName, program_file ) )
         {
            int index;
            if ( entry == NULL )
               index = 0;
            else
               index = rglCgGetProgramIndex( group, entry );
            if ( index >= 0 )
            {
               ret = rglpCgUpdateProgramAtIndex( group, index, 1 );
               break;
            }
            else
            {
               //we couldn't find the entry in the group which has the right name
               return ( CGprogram )NULL;
            }
         }
         group = group->next;
      }

      if ( ret )
         return ret;
      else
      {
         //do we have an elf file ?
         //read file tag:
         // check that file exists
         fp = fopen( program_file, "rb" );

         if ( NULL == fp )
         {
            rglCgRaiseError( CG_FILE_READ_ERROR );
            return ( CGprogram )NULL;
         }

         unsigned int filetag = 0;
         int res = fread( &filetag, sizeof( filetag ), 1, fp );
         if ( !res )
         {
            fclose( fp );
            rglCgRaiseError( CG_FILE_READ_ERROR );
            return ( CGprogram )NULL;
         }
         const unsigned int ElfTag = 0x7F454C46; // == MAKEFOURCC(0x7F,'E','L','F');
         if ( filetag == ElfTag )
         {
            fclose( fp );

            group = rglCgCreateProgramGroupFromFile( ctx, program_file );
            if ( group )
            {
               _CGprogramGroup *_group = ( _CGprogramGroup * )group;
               _group->userCreated = false;
               if ( entry == NULL )
               {
                  if ( group->programCount == 1 )
                  {
                     ret = rglpCgUpdateProgramAtIndex( group, 0, 1 );
                  }
               }
               else
               {
                  int index = rglCgGetProgramIndex( group, entry );
                  if ( index == -1 )
                  {
                     //_RGL_REPORT_EXTRA( RGL_REPORT_CG_ERROR, "couldn't find the shader entry in the CG binary" );
                  }
                  else
                  {
                     ret = rglpCgUpdateProgramAtIndex( group, index, 1 );
                  }
               }
            }
            return ret;
         }
         //else
         //rewind(); //we should rewind here, but no need since we are doing fseek after
      }
   }

   //we have a NV file or a CG source:
   if ( !fp )
   {
      fp = fopen( program_file, "rb" );
      if ( NULL == fp )
      {
         rglCgRaiseError( CG_FILE_READ_ERROR );
         return ( CGprogram )NULL;
      }
   }

   // find the file length
   size_t file_size = 0;
   fseek( fp, 0, SEEK_END );
   file_size = ftell( fp );
   rewind( fp );

   // alloc memory for the file
   char* ptr = ( char* )malloc( file_size + 1 );
   if ( NULL == ptr )
   {
      rglCgRaiseError( CG_MEMORY_ALLOC_ERROR );
      fclose( fp );
      return ( CGprogram )NULL;
   }

   // read the entire file into memory then close the file
   // TODO ********* just loading the file is a bit lame really. We can do better.
   fread( ptr, file_size, 1, fp );
   fclose( fp );

   if ( program_type == CG_SOURCE )
   {
      ptr[file_size] = '\0';
   }

   // call the CreateProgram API to do the rest of the job.
   CGprogram ret = cgCreateProgram( ctx, program_type, ptr, profile, entry, args );

   // free the memory for the file, we're done with it
   free( ptr );

   return ret;
}

CG_API CGprogram cgCopyProgram( CGprogram program )
{
   // check input parameter

   if ( !CG_IS_PROGRAM( program ) )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return NULL;
   }
   _CGprogram* prog = _cgGetProgPtr( program );
   if ( NULL == prog )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return ( CGprogram )NULL;
   }

   _CGprogram* newprog;
   size_t paddedProgramSize = 0;
   size_t ucodeSize = 0;

   if (prog->header.profile == CG_PROFILE_SCE_FP_TYPEB || prog->header.profile == CG_PROFILE_SCE_FP_RSX)
   {
      paddedProgramSize = rglPad( sizeof( _CGprogram ), 16);
      ucodeSize = prog->header.instructionCount * 16;
      newprog = ( _CGprogram* )malloc( paddedProgramSize + ucodeSize );
   }
   else
   {
      newprog = ( _CGprogram* )malloc( sizeof( _CGprogram ) );
   }

   if ( NULL == newprog )
   {
      rglCgRaiseError( CG_MEMORY_ALLOC_ERROR );
      return ( CGprogram )NULL;
   }
   rglCgProgramZero( newprog );

   // copy information from the old program
   newprog->header.profile = prog->header.profile;
   newprog->parentContext = prog->parentContext;

   // generate a new id
   newprog->id = ( CGprogram )rglCreateName( &_CurrentContext->cgProgramNameSpace, newprog );

   // copy all the parameter information


   // copy the binary program information
   // TODO ******** copy the entire elf here, not just the executable.
   int success = 0;
   switch ( prog->header.profile )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
         //case CG_PROFILE_SCE_VP_TYPEC:
      case CG_PROFILE_SCE_VP_RSX:
      case CG_PROFILE_SCE_FP_TYPEB:
         //case CG_PROFILE_SCE_FP_TYPEC:
      case CG_PROFILE_SCE_FP_RSX:
         success = rglpCopyProgram( prog, newprog );
         break;
      default:
         rglCgRaiseError( CG_UNKNOWN_PROFILE_ERROR );
         success = 0;
         break;
   }

   if ( success == 0 )
   {
      // we failed to create a new program object, clean up
      free( newprog );
      rglEraseName( &_CurrentContext->cgProgramNameSpace, (unsigned int)newprog->id );
      return ( CGprogram )NULL;
   }

   if (prog->header.profile == CG_PROFILE_SCE_FP_TYPEB || prog->header.profile == CG_PROFILE_SCE_FP_RSX)
   {
      newprog->ucode = (char*)newprog + paddedProgramSize;
      memcpy((char*)newprog->ucode, (char*)prog->ucode, ucodeSize);
   }

   //handle refcounting for string table and default values
   if ( prog->programGroup )
   {
      //refcount for the string and the default table
      newprog->programGroup = prog->programGroup;
      newprog->programIndexInGroup = -1;
      rglpCgUpdateProgramAtIndex( newprog->programGroup, -1, 1 );
   }

   // add the new program object to the program list in the context
   rglCgProgramPushFront( newprog->parentContext, newprog );

   if ( _cgProgramCopyHook ) _cgProgramCopyHook( newprog, prog );

   return newprog->id;
}


CG_API void cgDestroyProgram( CGprogram program )
{
   // remove the program from the program list

   // check the program input
   if ( !CG_IS_PROGRAM( program ) )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return;
   }
   _CGprogram* ptr = _cgGetProgPtr( program );
   if ( NULL == ptr )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return;
   }

   if ( ptr->programGroup )
   {
      if ( !ptr->programGroup->userCreated )
      {
         if ( ptr->programIndexInGroup != -1 && ptr->programGroup->programs[ptr->programIndexInGroup].refCount == 0 )
         {
            //not a valid handle, the refcount is 0, this handle has already been freed
            rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
            return;
         }
         else
         {
            bool isGroupMember = ( ptr->programIndexInGroup != -1 );
            rglpCgUpdateProgramAtIndex( ptr->programGroup, ptr->programIndexInGroup, -1 );
            if ( isGroupMember )
               return;
            //else continue, it needs to be deleted here, the group won't delete it.
            //such program which have a programGroup but are not member of the group are
            //clone of a program coming from a group. but those program still need the group
            //because the group holds their stringtable and their default table.
         }
      }
   }

   // get the context that this program belongs to.
   _CGcontext* ctx = ptr->parentContext;

   // find and unlink the program from the program list
   if ( ptr == ctx->programList )
   {
      // node is the head of the list, so unlink it.
      _CGprogram* p = ctx->programList;
      ctx->programList = p->next;
      // erase the program
      rglCgProgramEraseAndFree( p );
   }
   else
   {
      // node not the head, so use find_previous and delete_after
      // NOTE: if the program is not found in the list, returns silently.
      _CGprogram* p = rglCgProgramFindPrev( ctx, ptr );
      rglCgProgramEraseAfter( p );
   }
   return;
}


CG_API CGprogram cgGetFirstProgram( CGcontext ctx )
{
   if ( !CG_IS_CONTEXT( ctx ) )
   {
      rglCgRaiseError( CG_INVALID_CONTEXT_HANDLE_ERROR );
      return ( CGprogram )NULL;
   }
   // check context
   _CGcontext* c = _cgGetContextPtr( ctx );

   // return the id of the head of the program list in the context (got all that? good)
   _CGprogram* ptr = c->programList;
   if ( ptr )
   {
      // if any programs have been allocated...
      return c->programList->id;
   }

   return ( CGprogram )NULL;
}

CG_API CGprogram cgGetNextProgram( CGprogram current )
{
   // check the program input
   if ( !CG_IS_PROGRAM( current ) )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return NULL;
   }
   _CGprogram* ptr = _cgGetProgPtr( current );

   // increment the iterator down the program list
   if ( ptr->next != NULL )
   {
      return ptr->next->id;
   }

   // failed, so return an empty program
   return NULL;
}

CG_API CGcontext cgGetProgramContext( CGprogram prog )
{
   // check the program input
   if ( !CG_IS_PROGRAM( prog ) )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return NULL;
   }
   _CGprogram* ptr = _cgGetProgPtr( prog );

   return ptr->parentContext->id;
}

CG_API CGbool cgIsProgram( CGprogram program )
{
   if ( CG_IS_PROGRAM( program ) )
   {
      // the id was valid.
      return CG_TRUE;
   }
   // failed to find a valid id.
   return CG_FALSE;
}

CG_API void cgCompileProgram( CGprogram program )
{
   // check the program input
   if ( !CG_IS_PROGRAM( program ) )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return;
   }

   // TODO ****** use this function to re-link our program after creating parameter objects?

   return;
}

CG_API CGbool cgIsProgramCompiled( CGprogram program )
{
   // check the program input
   if ( !CG_IS_PROGRAM( program ) )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return CG_FALSE;
   }

   // TODO ********** use this function to find out if our program has unresolved symbols?

   return CG_TRUE;
}

CG_API void CGENTRY cgSetLastListing( CGhandle handle, const char *listing )
{
   return;
}

CG_API CGprofile cgGetProgramProfile( CGprogram prog )
{
   // check the program input
   if ( !CG_IS_PROGRAM( prog ) )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return CG_PROFILE_UNKNOWN;
   }

   // return the profile the program was compiled under
   return ( CGprofile )_cgGetProgPtr( prog )->header.profile;
}

CG_API int cgGetNumProgramDomains( CGprogram program )
{
   // check the program input
   if ( !CG_IS_PROGRAM( program ) )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return CG_PROFILE_UNKNOWN;
   }
   // under Jetstream, unlike GLSL, all programs have a single domain.
   return 1;
}

CG_API CGprogram cgCombinePrograms( int n, const CGprogram *exeList )
{
   // jetstream does not support combination of GLSL programs.
   return 0;
}

CG_API CGprogram cgCombinePrograms2( const CGprogram exe1, const CGprogram exe2 )
{
   // jetstream does not support combination of GLSL programs.
   return 0;
}

CG_API CGprogram cgCombinePrograms3( const CGprogram exe1, const CGprogram exe2, const CGprogram exe3 )
{
   // jetstream does not support combination of GLSL programs.
   return 0;
}

CG_API CGprofile cgGetProgramDomainProfile( CGprogram program, int index )
{
   // check the program input
   if ( !CG_IS_PROGRAM( program ) )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return CG_PROFILE_UNKNOWN;
   }

   if ( index >= 1 )
   {
      // jetstream programs can only have a single domain
      return CG_PROFILE_UNKNOWN;
   }

   // return the single profile under which the shader was compiled.
   return ( CGprofile )_cgGetProgPtr( program )->header.profile;
}

CG_API char const * const * cgGetProgramOptions( CGprogram program )
{
   // check the program input
   if ( !CG_IS_PROGRAM( program ) )
   {
      rglCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
      return NULL;
   }

   // NOTE: currently unsupported by Jetstream precompiled programs
   // TODO: get program options from ".note.MyShader" section of CG ELF Binary
   // or from compiler arguments of a runtime-compiled program.
   return NULL;
}

/*============================================================
  CG GL
  ============================================================ */

inline static float *rglGetUniformValuePtr( CGparameter param, CgRuntimeParameter *rtParameter )
{
   float* value = ( float* )( rtParameter->pushBufferPointer );

   // check in bounds to know if you should even bother checking that it is in an array
   if ( rtParameter > rtParameter->program->runtimeParameters )
   {
      CgRuntimeParameter *rtInArrayCheckParameter = rtParameter - 1;
      // check is array
      if ( rtInArrayCheckParameter->parameterEntry->flags & CGP_ARRAY )
      {
         value = *(( float** )( rtParameter->pushBufferPointer ) + CG_GETINDEX( param ) );
      }
   }
   return value;
}


// endian swapping of the fragment uniforms, if necessary
#if RGL_ENDIAN == RGL_BIG_ENDIAN
#define SWAP_IF_BE(arg) endianSwapWordByHalf(arg)
#else
#define SWAP_IF_BE(arg) arg
#endif


/******************************************************************************
 *** Profile Functions
 *****************************************************************************/

CGGL_API CGbool cgGLIsProfileSupported( CGprofile profile )
{
   //hack to counter removal of TypeC during beta
   if ( profile == ( CGprofile )7005 )
      profile = CG_PROFILE_SCE_VP_RSX;
   if ( profile == ( CGprofile )7006 )
      profile = CG_PROFILE_SCE_FP_RSX;

   switch ( profile )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
         //case CG_PROFILE_SCE_VP_TYPEC:
      case CG_PROFILE_SCE_VP_RSX:
         return ( CGbool ) rglpSupportsVertexProgram( profile );
      case CG_PROFILE_SCE_FP_TYPEB:
         //case CG_PROFILE_SCE_FP_TYPEC:
      case CG_PROFILE_SCE_FP_RSX:
         return ( CGbool ) rglpSupportsFragmentProgram( profile );
      default:
         return CG_FALSE;
   }
}

CGGL_API void cgGLEnableProfile( CGprofile profile )
{
   //hack to counter removal of TypeC during beta
   if ( profile == ( CGprofile )7005 )
      profile = CG_PROFILE_SCE_VP_RSX;
   if ( profile == ( CGprofile )7006 )
      profile = CG_PROFILE_SCE_FP_RSX;

   // this is a logical extension to glEnable
   RGLcontext* LContext = _CurrentContext;
   switch ( profile )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
      case CG_PROFILE_SCE_VP_RSX:
         LContext->VertexProgram = GL_TRUE;
         LContext->needValidate |= RGL_VALIDATE_VERTEX_PROGRAM;
         break;

      case CG_PROFILE_SCE_FP_TYPEB:
      case CG_PROFILE_SCE_FP_RSX:
         {
            LContext->FragmentProgram = GL_TRUE;
            struct _CGprogram* current = LContext->BoundFragmentProgram;
            if ( current )
            {
               for ( GLuint i = 0; i < current->samplerCount; ++i )
               {
                  int unit = current->samplerUnits[i];
                  rglTextureImageUnit *unit_ptr = (rglTextureImageUnit*)&_CurrentContext->TextureImageUnits[unit];
                  unit_ptr->currentTexture = rglGetCurrentTexture( unit_ptr, unit_ptr->fragmentTarget );
               }
            }
            LContext->needValidate |= PSGL_VALIDATE_FRAGMENT_PROGRAM | PSGL_VALIDATE_TEXTURES_USED;
         }
         break;
      default:
         rglCgRaiseError( CG_INVALID_PROFILE_ERROR );
         break;
   }
}

CGGL_API void cgGLDisableProfile( CGprofile profile )
{
   //hack to counter removal of TypeC during beta
   if ( profile == ( CGprofile )7005 )
      profile = CG_PROFILE_SCE_VP_RSX;
   if ( profile == ( CGprofile )7006 )
      profile = CG_PROFILE_SCE_FP_RSX;

   // this is a logical extension to glDisable
   RGLcontext* LContext = _CurrentContext;
   switch ( profile )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
         //case CG_PROFILE_SCE_VP_TYPEC:
      case CG_PROFILE_SCE_VP_RSX:
         LContext->VertexProgram = GL_FALSE;
         // no need to invalidate textures because they are only available on programmable pipe.
         LContext->needValidate |= RGL_VALIDATE_VERTEX_PROGRAM ;
         break;
      case CG_PROFILE_SCE_FP_TYPEB:
         //case CG_PROFILE_SCE_FP_TYPEC:
      case CG_PROFILE_SCE_FP_RSX:
         LContext->FragmentProgram = GL_FALSE;
         break;
      default:
         rglCgRaiseError( CG_INVALID_PROFILE_ERROR );
         break;
   }
}


CGGL_API CGprofile cgGLGetLatestProfile( CGGLenum profile_type )
{
   switch ( profile_type )
   {
      case CG_GL_VERTEX:
      case CG_GL_FRAGMENT:
         return rglpGetLatestProfile( profile_type );
      default:
         rglCgRaiseError( CG_INVALID_ENUMERANT_ERROR );
         return CG_PROFILE_UNKNOWN;
   }
}

CGGL_API void cgGLSetOptimalOptions( CGprofile profile )
{
   // This does nothing because we don't compile at run-time.
}

/******************************************************************************
 *** Program Managment Functions
 *****************************************************************************/

CGGL_API void cgGLLoadProgram( CGprogram program )
{
   // noop since there is integration of cg in js.
}

CGGL_API CGbool cgGLIsProgramLoaded( CGprogram program )
{
   // TODO: for now a valid program is always loaded.
   return CG_TRUE;
}


CGGL_API void cgGLBindProgram( CGprogram program )
{
   _CGprogram* ptr = _cgGetProgPtr( program );

   // now do the binding.
   switch ( ptr->header.profile )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
         //hack to counter removal of TypeC during beta
      case 7005:
      case CG_PROFILE_SCE_VP_RSX:
         // the program is a vertex program, just update the GL state
         _CurrentContext->BoundVertexProgram = ptr;

         // and inform the GL state to re-upload the vertex program
         _CurrentContext->needValidate |= PSGL_VALIDATE_VERTEX_PROGRAM;
         break;

      case CG_PROFILE_SCE_FP_TYPEB:
         //hack to counter removal of TypeC during beta
      case 7006:
      case CG_PROFILE_SCE_FP_RSX:
         _CurrentContext->BoundFragmentProgram = ptr;

         // need to revalidate the textures in order to update which targets to fetch from
         _CurrentContext->needValidate |= PSGL_VALIDATE_FRAGMENT_PROGRAM | PSGL_VALIDATE_TEXTURES_USED;

         // TODO: push texture state
         //  Needs to be done per profile. Can't use glPushAttrib.

         // deal with the texture parameters now.
         for ( GLuint index = 0; index < ptr->samplerCount; ++index )
         {
            // walk the array of sampler parameters
            CgRuntimeParameter *rtParameter = ptr->runtimeParameters + ptr->samplerIndices[index];
            CgParameterResource *parameter = ( CgParameterResource * )( ptr->parameterResources + rtParameter->parameterEntry->typeIndex );
            // find out which texture unit this parameter has been assigned to
            unsigned int unit = parameter->resource - CG_TEXUNIT0;
            _CurrentContext->TextureImageUnits[unit].fragmentTarget = rtParameter->glType;
            rglTextureImageUnit *unit_ptr = (rglTextureImageUnit*)&_CurrentContext->TextureImageUnits[unit];
            unit_ptr->currentTexture = rglGetCurrentTexture( unit_ptr, unit_ptr->fragmentTarget );
         }
         break;

      default:
         rglCgRaiseError( CG_INVALID_PROFILE_ERROR );
         return;
   }

}

CGGL_API void cgGLUnbindProgram( CGprofile profile )
{
   switch ( profile )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
         //case CG_PROFILE_SCE_VP_TYPEC:
      case CG_PROFILE_SCE_VP_RSX:
         //hack to counter removal of TypeC during beta
      case 7005:
         _CurrentContext->BoundVertexProgram = NULL;
         _CurrentContext->needValidate |= PSGL_VALIDATE_VERTEX_PROGRAM;
         // no need to invalidate textures because they are only available on programmable pipe.
         break;
      case CG_PROFILE_SCE_FP_TYPEB:
         //case CG_PROFILE_SCE_FP_TYPEC:
      case CG_PROFILE_SCE_FP_RSX:
         //hack to counter removal of TypeC during beta
      case 7006:
         _CurrentContext->BoundFragmentProgram = NULL;
         break;
      default:
         rglCgRaiseError( CG_INVALID_PROFILE_ERROR );
         return;
   }

}


// this API exposes internal implementation of Cg.
// Since we do not rely on program objects, always return 0.
CGGL_API GLuint cgGLGetProgramID( CGprogram program )
{
   return 0;
}

CGGL_API void cgGLEnableProgramProfiles( CGprogram program )
{
   // TODO: unsupported in Jetstream
   return;
}

CGGL_API void cgGLDisableProgramProfiles( CGprogram program )
{
   // TODO: unsupported in Jetstream
   return;
}


/******************************************************************************
 *** Parameter Managment Functions
 *****************************************************************************/

CGGL_API void cgGLSetParameter1f( CGparameter param, float x )
{
   // check to see if the parameter is a good one
   CgRuntimeParameter *ptr = rglCgGLTestParameter( param );

   // otherwise apply the values to the parameter
   float v[4] = {x, x, x, x};
   ptr->setterIndex( ptr, v, CG_GETINDEX( param ) );
}

CGGL_API void cgGLSetParameter2f( CGparameter param, float x, float y )
{
   CgRuntimeParameter *ptr = rglCgGLTestParameter( param );

   // otherwise apply the values to the parameter
   float v[4] = {x, y, y, y};
   ptr->setterIndex( ptr, v, CG_GETINDEX( param ) );
}

CGGL_API void cgGLSetParameter3f( CGparameter param, float x, float y, float z )
{
   CgRuntimeParameter *ptr = rglCgGLTestParameter( param );

   // otherwise apply the values to the parameter
   float v[4] = {x, y, z, z};
   ptr->setterIndex( ptr, v, CG_GETINDEX( param ) );
}

CGGL_API void cgGLSetParameter4f( CGparameter param, float x, float y, float z, float w )
{
   CgRuntimeParameter *ptr = rglCgGLTestParameter( param );

   // otherwise apply the values to the parameter
   float v[4] = {x, y, z, w};
   ptr->setterIndex( ptr, v, CG_GETINDEX( param ) );
}

CGGL_API void cgGLSetParameter1fv( CGparameter param, const float *v )
{
   CgRuntimeParameter *ptr = rglCgGLTestParameter( param );

   float v2[4] = { v[0], v[0], v[0], v[0]};
   ptr->setterIndex( ptr, v2, CG_GETINDEX( param ) );
}

CGGL_API void cgGLSetParameter2fv( CGparameter param, const float *v )
{
   CgRuntimeParameter *ptr = rglCgGLTestParameter( param );

   float v2[4] = { v[0], v[1], v[1], v[1]};
   ptr->setterIndex( ptr, v2, CG_GETINDEX( param ) );
}

CGGL_API void cgGLSetParameter3fv( CGparameter param, const float *v )
{
   CgRuntimeParameter *ptr = rglCgGLTestParameter( param );

   float v2[4] = { v[0], v[1], v[2], v[2]};
   ptr->setterIndex( ptr, v2, CG_GETINDEX( param ) );
}

CGGL_API void cgGLSetParameter4fv( CGparameter param, const float *v )
{
   CgRuntimeParameter *ptr = rglCgGLTestParameter( param );

   float v2[4] = { v[0], v[1], v[2], v[3]};
   ptr->setterIndex( ptr, v2, CG_GETINDEX( param ) );
}

CGGL_API void cgGLGetParameter1f( CGparameter param, float *v )
{
   //check parameter handle
   CgRuntimeParameter *rtParameter = rglCgGLTestParameter( param );
   if ( !rtParameter )
      return;

   const CgParameterEntry *parameterEntry = rtParameter->parameterEntry;
   if (( parameterEntry->flags & CGP_TYPE_MASK ) != CGP_INTRINSIC ||
         (( parameterEntry->flags & CGPV_MASK ) != CGPV_UNIFORM && ( parameterEntry->flags & CGPV_MASK ) != CGPV_CONSTANT ) )
   {
      rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
      return;
   }

   // uniforms only
   float* value = rglGetUniformValuePtr( param, rtParameter );
   if ( !value )
   {
      rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
      return;
   }

   const CgParameterResource *parameterResource = rglGetParameterResource( rtParameter->program, parameterEntry );

   switch ( parameterResource->type )
   {
      case CG_FLOAT4:
      case CG_FLOAT3:
      case CG_FLOAT2:
      case CG_FLOAT:
      case CG_HALF4:
      case CG_HALF3:
      case CG_HALF2:
      case CG_HALF:
      case CG_INT4:
      case CG_INT3:
      case CG_INT2:
      case CG_INT:
      case CG_BOOL4:
      case CG_BOOL3:
      case CG_BOOL2:
      case CG_BOOL:
      case CG_FIXED4:
      case CG_FIXED3:
      case CG_FIXED2:
      case CG_FIXED:
         *v = *value; // fall through...
         break;
      default:
         rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
   }
}

CGGL_API void cgGLGetParameter2f( CGparameter param, float *v )
{
   //check parameter handle
   CgRuntimeParameter *rtParameter = rglCgGLTestParameter( param );
   if ( !rtParameter )
      return;

   const CgParameterEntry *parameterEntry = rtParameter->parameterEntry;
   if (( parameterEntry->flags & CGP_TYPE_MASK ) != CGP_INTRINSIC ||
         (( parameterEntry->flags & CGPV_MASK ) != CGPV_UNIFORM && ( parameterEntry->flags & CGPV_MASK ) != CGPV_CONSTANT ) )
   {
      rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
      return;
   }

   // uniforms only
   float* value = rglGetUniformValuePtr( param, rtParameter );
   if ( !value )
   {
      rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
      return;
   }

   // peek into image for value
   const CgParameterResource *parameterResource = rglGetParameterResource( rtParameter->program, parameterEntry );
   switch ( parameterResource->type )
   {
      case CG_FLOAT2:
      case CG_FLOAT3:
      case CG_FLOAT4:
      case CG_HALF2:
      case CG_HALF3:
      case CG_HALF4:
      case CG_INT2:
      case CG_INT3:
      case CG_INT4:
      case CG_BOOL2:
      case CG_BOOL3:
      case CG_BOOL4:
      case CG_FIXED2:
      case CG_FIXED3:
      case CG_FIXED4:
         v[0] = value[0];
         v[1] = value[1];
         break;
      default:
         rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
         break;
   }
}

CGGL_API void cgGLGetParameter3f( CGparameter param, float *v )
{
   //check parameter handle
   CgRuntimeParameter *rtParameter = rglCgGLTestParameter( param );
   if ( !rtParameter )
      return;

   const CgParameterEntry *parameterEntry = rtParameter->parameterEntry;
   if (( parameterEntry->flags & CGP_TYPE_MASK ) != CGP_INTRINSIC ||
         (( parameterEntry->flags & CGPV_MASK ) != CGPV_UNIFORM && ( parameterEntry->flags & CGPV_MASK ) != CGPV_CONSTANT ) )
   {
      rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
      return;
   }

   // uniforms only
   float* value = rglGetUniformValuePtr( param, rtParameter );
   if ( !value )
   {
      rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
      return;
   }

   // peek into image for value
   const CgParameterResource *parameterResource = rglGetParameterResource( rtParameter->program, parameterEntry );
   switch ( parameterResource->type )
   {
      case CG_FLOAT3:
      case CG_FLOAT4:
      case CG_HALF3:
      case CG_HALF4:
      case CG_INT3:
      case CG_INT4:
      case CG_BOOL3:
      case CG_BOOL4:
      case CG_FIXED3:
      case CG_FIXED4:
         v[0] = value[0];
         v[1] = value[1];
         v[2] = value[2];
         break;
      default:
         rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
         break;
   }
}

CGGL_API void cgGLGetParameter4f( CGparameter param, float *v )
{
   //check parameter handle
   CgRuntimeParameter *rtParameter = rglCgGLTestParameter( param );
   if ( !rtParameter )
      return;

   const CgParameterEntry *parameterEntry = rtParameter->parameterEntry;
   if (( parameterEntry->flags & CGP_TYPE_MASK ) != CGP_INTRINSIC ||
         (( parameterEntry->flags & CGPV_MASK ) != CGPV_UNIFORM && ( parameterEntry->flags & CGPV_MASK ) != CGPV_CONSTANT ) )
   {
      rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
      return;
   }

   // uniforms only
   float* value = rglGetUniformValuePtr( param, rtParameter );
   if ( !value )
   {
      rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
      return;
   }

   // peek into image for value
   const CgParameterResource *parameterResource = rglGetParameterResource( rtParameter->program, parameterEntry );
   switch ( parameterResource->type )
   {
      case CG_FLOAT4:
      case CG_HALF4:
      case CG_INT4:
      case CG_BOOL4:
      case CG_FIXED4:
         v[0] = value[0];
         v[1] = value[1];
         v[2] = value[2];
         v[3] = value[3];
         break;
      default:
         rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
         break;
   }
}

CGGL_API void cgGLSetParameterArray1f( CGparameter param,
      long offset,
      long nelements,
      const float *v )
{
   CgRuntimeParameter* ptr = _cgGLTestArrayParameter( param, offset, nelements );


   if ( nelements == 0 )
   {
      const CgParameterArray *parameterArray = rglGetParameterArray( ptr->program, ptr->parameterEntry );
      nelements = rglGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount ) - offset;
   }

   //we have an array here, the parameterEntry of the type is the next one
   ptr++;

   // loop over array elements
   for ( int i = 0; i < nelements; ++i )
   {
      ptr->setterIndex( ptr, v + i, i + offset );
   }
}

CGGL_API void cgGLSetParameterArray2f( CGparameter param,
      long offset,
      long nelements,
      const float *v )
{
   CgRuntimeParameter* ptr = _cgGLTestArrayParameter( param, offset, nelements );

   if ( nelements == 0 )
   {
      const CgParameterArray *parameterArray = rglGetParameterArray( ptr->program, ptr->parameterEntry );
      nelements = rglGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount ) - offset;
   }

   //we have an array here, the parameterEntry of the type is the next one
   ptr++;

   // loop over array elements
   for ( int i = 0; i < nelements; ++i )
   {
      ptr->setterIndex( ptr, v + 2 * i, i + offset );
   }
}

CGGL_API void cgGLSetParameterArray3f( CGparameter param,
      long offset,
      long nelements,
      const float *v )
{

   CgRuntimeParameter* ptr = _cgGLTestArrayParameter( param, offset, nelements );
   if ( nelements == 0 )
   {
      const CgParameterArray *parameterArray = rglGetParameterArray( ptr->program, ptr->parameterEntry );
      nelements = rglGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount ) - offset;
   }

   //we have an array here, the parameterEntry of the type is the next one
   ptr++;
   // loop over array elements
   for ( int i = 0; i < nelements; ++i )
   {
      ptr->setterIndex( ptr, v + 3 * i, i + offset );
   }
}

CGGL_API void cgGLSetParameterArray4f( CGparameter param,
      long offset,
      long nelements,
      const float *v )
{

   CgRuntimeParameter* ptr = _cgGLTestArrayParameter( param, offset, nelements );
   if ( nelements == 0 )
   {
      const CgParameterArray *parameterArray = rglGetParameterArray( ptr->program, ptr->parameterEntry );
      nelements = rglGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount ) - offset;
   }

   //we have an array here, the parameterEntry of the type is the next one
   ptr++;
   // loop over array elements
   for ( int i = 0; i < nelements; ++i )
   {
      ptr->setterIndex( ptr, v + 4 * i, i + offset );
   }
}

CGGL_API void cgGLGetParameterArray1f( CGparameter param,
      long offset,
      long nelements,
      float *v )
{
   if ( nelements == 0 )  nelements = cgGetArraySize( param, 0 ) - offset;
   // loop over array elements
   for ( int i = 0; i < nelements; ++i )
   {
      CGparameter p = cgGetArrayParameter( param, i + offset );
      cgGLGetParameter1f( p, v + i );
   }
}

CGGL_API void cgGLGetParameterArray2f( CGparameter param,
      long offset,
      long nelements,
      float *v )
{
   if ( nelements == 0 )  nelements = cgGetArraySize( param, 0 ) - offset;
   // loop over array elements
   for ( int i = 0; i < nelements; ++i )
   {
      CGparameter p = cgGetArrayParameter( param, i + offset );
      cgGLGetParameter2f( p, v + 2*i );
   }
}

CGGL_API void cgGLGetParameterArray3f( CGparameter param,
      long offset,
      long nelements,
      float *v )
{

   if ( nelements == 0 )  nelements = cgGetArraySize( param, 0 ) - offset;
   // loop over array elements
   for ( int i = 0; i < nelements; ++i )
   {
      CGparameter p = cgGetArrayParameter( param, i + offset );
      cgGLGetParameter3f( p, v + 3*i );
   }
}

CGGL_API void cgGLGetParameterArray4f( CGparameter param,
      long offset,
      long nelements,
      float *v )
{

   if ( nelements == 0 )  nelements = cgGetArraySize( param, 0 ) - offset;
   // loop over array elements
   for ( int i = 0; i < nelements; ++i )
   {
      CGparameter p = cgGetArrayParameter( param, i + offset );
      cgGLGetParameter4f( p, v + 4*i );
   }
}

CGGL_API void cgGLSetParameterPointer( CGparameter param,
      GLint fsize,
      GLenum type,
      GLsizei stride,
      const GLvoid *pointer )
{
   CgRuntimeParameter *_ptr = rglCgGLTestParameter( param );

   const CgParameterResource *parameterResource = rglGetParameterResource( _ptr->program, _ptr->parameterEntry );
   GLuint index = ( GLuint )( parameterResource->resource - CG_ATTR0 );

   rglVertexAttribPointerNV(
         index,    // attribute index
         fsize,                  // element size
         type,                   // GL_FLOAT
         ( _ptr->parameterEntry->flags & CGP_NORMALIZE ) ? 1 : 0,  // from the semantic of the param
         stride,                 // element to element in bytes
         pointer );              // data pointer
}

CGGL_API void cgGLEnableClientState( CGparameter param )
{
   CgRuntimeParameter *_ptr = rglCgGLTestParameter( param );

   const CgParameterResource *parameterResource = rglGetParameterResource( _ptr->program, _ptr->parameterEntry );

   GLuint index = ( GLuint )( parameterResource->resource - CG_ATTR0 );
   rglEnableVertexAttribArrayNV( index );
}

CGGL_API void cgGLDisableClientState( CGparameter param )
{
   CgRuntimeParameter *_ptr = rglCgGLTestParameter( param );

   const CgParameterResource *parameterResource = rglGetParameterResource( _ptr->program, _ptr->parameterEntry );

   GLuint index = ( GLuint )( parameterResource->resource - CG_ATTR0 );
   rglDisableVertexAttribArrayNV( index );
}

/******************************************************************************
 *** Matrix Parameter Managment Functions
 *****************************************************************************/

CGGL_API void cgGLSetMatrixParameterfc( CGparameter param, const float *matrix )
{

   CgRuntimeParameter* ptr = rglCgGLTestParameter( param );
   ptr->settercIndex( ptr, matrix, CG_GETINDEX( param ) );
}

CGGL_API void cgGLSetTextureParameter( CGparameter param, GLuint texobj )
{
   // According to the cg implementation from nvidia, set just stores the obj.
   // Enable does the actual work.
   CgRuntimeParameter* ptr = _cgGLTestTextureParameter( param );

   ptr->samplerSetter( ptr, &texobj, 0 );
}

CGGL_API GLuint cgGLGetTextureParameter( CGparameter param )
{
   CgRuntimeParameter* ptr = _cgGLTestTextureParameter( param );
   if ( ptr == NULL ) return 0;
   if ( !( ptr->parameterEntry->flags & CGPF_REFERENCED ) ) { rglCgRaiseError( CG_INVALID_PARAMETER_ERROR ); return 0; }
   return *( GLuint* )ptr->pushBufferPointer;
   return 0;
}

CGGL_API void cgGLEnableTextureParameter( CGparameter param )
{
   CgRuntimeParameter* ptr = _cgGLTestTextureParameter( param );
   ptr->samplerSetter( ptr, NULL, 0 );
}

CGGL_API void cgGLDisableTextureParameter( CGparameter param )
{
   if ( _cgGLTestTextureParameter( param ) )
   {
      // this does not do anything on nvidia's implementation
   }
}

CGGL_API GLenum cgGLGetTextureEnum( CGparameter param )
{
   // The returned value is the texture unit assigned to this parameter
   // by the Cg compiler.
   CgRuntimeParameter* ptr = _cgGLTestTextureParameter( param );
   if ( ptr == NULL ) return GL_INVALID_OPERATION;

   if ( ptr->parameterEntry->flags & CGP_RTCREATED )
   {
      // runtime created texture parameters do not have allocated texture units
      rglCgRaiseError( CG_INVALID_PARAMETER_ERROR );
      return GL_INVALID_OPERATION;
   }

   // XXX  what about the vertex texture enums !?
   if (( ptr->program->header.profile == CG_PROFILE_SCE_VP_TYPEB )
         //|| (ptr->program->header.profile==CG_PROFILE_SCE_VP_TYPEC)
         || ( ptr->program->header.profile == CG_PROFILE_SCE_VP_RSX ) )
   {
      return GL_INVALID_OPERATION;
   }

   if ( !( ptr->parameterEntry->flags & CGPF_REFERENCED ) || !(( ptr->parameterEntry->flags & CGPV_MASK ) == CGPV_UNIFORM ) ) { rglCgRaiseError( CG_INVALID_PARAMETER_ERROR ); return GL_INVALID_OPERATION; }
   const CgParameterResource *parameterResource = rglGetParameterResource( ptr->program, ptr->parameterEntry );
   return GL_TEXTURE0 + parameterResource->resource - CG_TEXUNIT0;
}

CGGL_API void cgGLSetDebugMode( CGbool debug )
{
}

/*============================================================
  CG NV2ELF
  ============================================================ */

using namespace cgc::bio;

class CgBaseType
{
   public:
      CgBaseType() {_type = 0;}
      virtual ~CgBaseType() {}
      unsigned short _type; //index of the type in the types array of the program if >= STRUCT, else it's a BASETYPE
      short _resourceIndex; //index of the type in the types array of the program if >= STRUCT, else it's a BASETYPE
      unsigned short _resource; //index of the type in the types array of the program if >= STRUCT, else it's a BASETYPE
};

//array
class CgArrayType : public CgBaseType
{
   public:
      CgArrayType():_elementType(NULL) { _type = (unsigned char)(CG_ARRAY + 128); }
      virtual ~CgArrayType()
      {
         if (_elementType)
            delete _elementType;
      }
      CgBaseType *_elementType;
      unsigned char _dimensionCount;
      unsigned short _dimensionItemCountsOffset;
};

//structure
class CgStructureType : public CgBaseType
{
   public:
      //definitions
      class CgStructuralElement
      {
         public:
            char _name[256];
            char _semantic[256];
            CgBaseType *_type;
            unsigned short _flags; //in,out,global,varying,const,uniform,shared, hasDefaults(base types only) -> consecutive reg allocation flag
            unsigned short _index; //helper index ?
      };

      //implementation
      CgStructureType()
      {
         _type = (unsigned char)(CG_STRUCT + 128);
         _root = false;
      }

      virtual ~CgStructureType()
      {
         int i=0;
         int count = (int)_elements.size();
         for (i=0;i<count;i++)
         {
            if (_elements[i]._type)
               delete _elements[i]._type;
         }
      }

      //members
      std::vector<CgStructuralElement> _elements;
      bool _insideArray; //modifies the way the resources are stored, they should be sent directly to the resources,
      //the array is keeping track of the resource offset in that case
      bool _root;
};

typedef struct
{
   const char *name;
   int resIndex;
} BINDLOCATION;



//////////////////////////////////////////////////////////////////////////////
// static arrays containing type information

#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_enum, nrows, ncols,classname) \
   classname ,
static int classnames[] = {
#include "Cg/cg_datatypes.h"
};

#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_enum, nrows, ncols,classname) \
   nrows ,
static int rows[] = {
#include "Cg/cg_datatypes.h"
};

typedef float _float4[4];

//////////////////////////////////////////////////////////////////////////////
// static buffers containing the information extracted from the shaders

typedef struct
{
   std::vector<short> _resources;
   std::vector<unsigned short> _defaultValuesIndices;
   std::vector<unsigned short> _elfDefaultsIndices;
   std::vector<short> _dimensions;
   std::vector<CgParameterSemantic> _semanticIndices;
} _CGNVCONTAINERS;

static bool bIsVertexProgram = true;


//////////////////////////////////////////////////////////////////////////////
// forward declarations

static int getStride(CgBaseType *type);
static int getSizeofSubArray(_CGNVCONTAINERS &containers, int dimensionIndex, int dimensionCount, int endianness);

static unsigned int stringTableFind( std::vector<char> &stringTable, const char* str  )
{
   const char* data = &stringTable[0];
   size_t size = stringTable.size();
   const char *end = data + size;

   size_t length = strlen(str);
   if (length+1 > size)
      return 0;
   data += length;

   const char *p = (char*)memchr(data,'\0',end-data);
   while (p && (end-data)>0)
   {
      if (!memcmp(p - length, str, length))
         return (unsigned int)(p - length - &stringTable[0]);
      data = p+1;	
      p = (char*)memchr(data,'\0',end-data);
   }
   return 0;
}

static unsigned int stringTableAdd( std::vector<char> &stringTable, const char* str )
{
   unsigned int ret = (unsigned int)stringTable.size();

   if ( ret == 0 )
   {
      stringTable.push_back('\0'); //add the empty string
      ret = 1;
   }

   size_t stringLength = strlen(str) + 1;
   stringTable.resize(ret + stringLength);
   memcpy(&stringTable[0] + ret,str,stringLength);

   return ret;
}

static unsigned int stringTableAddUnique( std::vector<char> &stringTable, const char* str )
{
   if ( stringTable.size() == 0 )
      stringTable.push_back('\0'); //add the empty string
   unsigned int ret = stringTableFind(stringTable, str);
   if (ret == 0 && str[0] != '\0')
      ret = stringTableAdd(stringTable, str);
   return ret;
}

template<class Type> static void array_push(char* &parameterOffset, std::vector<Type> &array);
static unsigned short getFlags(CGenum var, CGenum dir, int no,	bool is_referenced, bool is_shared, int paramIndex);

//////////////////////////////////////////////////////////////////////////////
// implementation

static void fillStructureItems(_CGNVCONTAINERS &containers, CgStructureType *structure,
      int endianness,
      std::vector<CgParameterEntry> &parameterEntries,
      std::vector<char> &parameterResources, std::vector<char> &stringTable, unsigned short *arrayResourceIndex = NULL,
      unsigned short *arrayDefaultValueIndex = NULL);


int convertNvToElfFromFile(const char *sourceFile, int endianness, int constTableOffset, void **binaryShader, int *size,
      std::vector<char> &stringTable, std::vector<float> &defaultValues)
{
   FILE *fp = fopen(sourceFile, "rb");
   if (fp)
   {
      fseek(fp,0,SEEK_END);
      size_t filesize = ftell(fp);
      fseek(fp,0,SEEK_SET);
      void *data = malloc(filesize);
      if (data == NULL)
      {
         fclose(fp);
         //RGL_ASSERT2(0,("not enough memory to read the shader source"));
         return -2;
      }
      fread(data,filesize,1,fp);
      fclose(fp);

      int res = convertNvToElfFromMemory(data,filesize,endianness,constTableOffset, binaryShader, size,stringTable,defaultValues);
      free(data);
      return res;
   }
   //RGL_ASSERT2(0,("couldn't open source file %s\n",sourceFile));
   return -1;
}

static inline uint32_t swap16(const uint32_t v)
{
   return (v>>16) | (v<<16);
}

int convertNvToElfFromMemory(const void *sourceData, size_t size, int endianness, int constTableOffset, void **binaryShader, int *binarySize,
      std::vector<char> &stringTable, std::vector<float> &defaultValues)
{
   _CGNVCONTAINERS containers;

   unsigned char elfEndianness = endianness; //used in the macro CNVEND

   nvb_reader* nvbr = 0;
   bin_io::instance()->new_nvb_reader( &nvbr );
   CGBIO_ERROR err = nvbr->loadFromString((const char*)sourceData,size);
   if (err != CGBIO_ERROR_NO_ERROR)
      return -1;

   //flag which tells if we have to swap data coming from nv and going to elf
   bool doSwap = !(nvbr->endianness() == (HOST_ENDIANNESS)elfEndianness);

   CGprofile NVProfile = nvbr->profile();
   //hack to counter removal of TypeC during beta
   if (NVProfile == (CGprofile)7005 )
      NVProfile = CG_PROFILE_SCE_VP_RSX;
   if (NVProfile == (CGprofile)7006 )
      NVProfile = CG_PROFILE_SCE_FP_RSX;
   if (NVProfile == CG_PROFILE_SCE_VP_TYPEB || NVProfile == CG_PROFILE_SCE_VP_RSX)
   {
      bIsVertexProgram = true;
   }
   else if (NVProfile == CG_PROFILE_SCE_FP_TYPEB || NVProfile == CG_PROFILE_SCE_FP_RSX)
   {
      bIsVertexProgram = false;
   }
   else
   {
      //RGL_ASSERT2(0,("error: unknown shader profile\n"));
      return -1;
   }

   //Fill the shader header structure and save it into the shadertab
   CgProgramHeader cgShader;
   memset(&cgShader,0,sizeof(CgProgramHeader));
   cgShader.profile = CNV2END((unsigned short) NVProfile); //ok cnv2end here we go from platform endiannes to elf endiannes
   cgShader.compilerVersion = 0;//TODO
   if (bIsVertexProgram)
   {
      const CgBinaryVertexProgram *nvVertex = nvbr->vertex_program();
      if (doSwap) //here we go directly from nv to elf endiannes without going to the platform endianness
      {
         cgShader.instructionCount = ENDSWAP(nvVertex->instructionCount);
         cgShader.attributeInputMask = ENDSWAP(nvVertex->attributeInputMask);
         //vertex program specific
         cgShader.vertexProgram.instructionSlot = ENDSWAP(nvVertex->instructionSlot);
         cgShader.vertexProgram.registerCount = ENDSWAP(nvVertex->registerCount);
         cgShader.vertexProgram.attributeOutputMask = ENDSWAP(nvVertex->attributeOutputMask);
      }
      else
      {
         cgShader.instructionCount = nvVertex->instructionCount;
         cgShader.attributeInputMask = nvVertex->attributeInputMask;
         //vertex program specific
         cgShader.vertexProgram.instructionSlot = nvVertex->instructionSlot;
         cgShader.vertexProgram.registerCount = nvVertex->registerCount;
         cgShader.vertexProgram.attributeOutputMask = nvVertex->attributeOutputMask;
      }
   }
   else
   {
      const CgBinaryFragmentProgram *nvFragment = nvbr->fragment_program();
      unsigned char flags;
      if (doSwap)
      {
         cgShader.instructionCount = ENDSWAP(nvFragment->instructionCount);
         cgShader.attributeInputMask = ENDSWAP(nvFragment->attributeInputMask);
         //fragment program specific
         cgShader.fragmentProgram.partialTexType = ENDSWAP(nvFragment->partialTexType);
         cgShader.fragmentProgram.texcoordInputMask = ENDSWAP(nvFragment->texCoordsInputMask);
         cgShader.fragmentProgram.texcoord2d = ENDSWAP(nvFragment->texCoords2D);
         cgShader.fragmentProgram.texcoordCentroid = ENDSWAP(nvFragment->texCoordsCentroid);
         cgShader.fragmentProgram.registerCount = ENDSWAP(nvFragment->registerCount);
         flags =
            (nvFragment->outputFromH0 ? CGF_OUTPUTFROMH0 : 0) |
            (nvFragment->depthReplace ? CGF_DEPTHREPLACE : 0) |
            (nvFragment->pixelKill ? CGF_PIXELKILL : 0);
      }
      else
      {
         cgShader.instructionCount = nvFragment->instructionCount;
         cgShader.attributeInputMask = nvFragment->attributeInputMask;
         //fragment program specific
         cgShader.fragmentProgram.partialTexType = nvFragment->partialTexType;
         cgShader.fragmentProgram.texcoordInputMask = nvFragment->texCoordsInputMask;
         cgShader.fragmentProgram.texcoord2d = nvFragment->texCoords2D;
         cgShader.fragmentProgram.texcoordCentroid = nvFragment->texCoordsCentroid;
         cgShader.fragmentProgram.registerCount = nvFragment->registerCount;
         flags =
            (nvFragment->outputFromH0 ? CGF_OUTPUTFROMH0 : 0) |
            (nvFragment->depthReplace ? CGF_DEPTHREPLACE : 0) |
            (nvFragment->pixelKill ? CGF_PIXELKILL : 0);
      }
      cgShader.fragmentProgram.flags = CNV2END(flags);
   }

   //shader specific info ( the shader header is in the shader tab )

   //ucode
   uint32_t *tmp = (uint32_t *)nvbr->ucode();
   const char *ucode;
   uint32_t *buffer = NULL;
   if (doSwap)
   {
      int size = (int)nvbr->ucode_size()/sizeof(uint32_t);
      buffer = new uint32_t[size];

      for (int i = 0; i < size; i++)
      {
         uint32_t val = ENDSWAP(tmp[i]);
         if (!bIsVertexProgram)
            val = swap16(val);
         buffer[i] = val;
      }
      ucode = (const char*)buffer;
   }
   else
   {
      ucode = (const char*)tmp;
      // !!!xxx this is to workaround what appears to be a linux platform specific bug
      // that manifests as a memory overwrite in properly allocated memory during a std::vector resize
      int size = (int)nvbr->ucode_size()/sizeof(uint32_t);
      buffer = new uint32_t[size];

      for (int i = 0; i < size; i++)
         buffer[i] = tmp[i];
      ucode = (const char*)buffer;
      // end workaround
   }

   int ucodeSize = nvbr->ucode_size();

   //ucode, ucodeSize;

   //RGL_ASSERT2(CNV2END(cgShader.instructionCount)*16 ==nvbr->ucode_size(),("%i, %i",CNV2END(cgShader.instructionCount)*16,nvbr->ucode_size()));

   //parameters
   CgStructureType root;
   root._insideArray = false;
   root._root = true;

   //we don't have the names of the user structures, so to share structures we will have to rely on the
   //layout of the data, we might do mistakes, but it's not important, since what we want to represent is
   //the structure layout.
   //parameters
   //the parameters represents a hierarchical structure, I am reconstructing this structure

   int paramIndex = -1;

   //just for sanity check purpose: it's use to detect when there's a change of structural element to reset the embedded constant counter
   CgStructureType::CgStructuralElement *current = NULL;
   int embeddedConstants = 0;
   bool rootChildDefaultValues = false;
   int rootChildIndex = -1;

   char currentRefStructureName[256];
   currentRefStructureName[0] = '\0';

   int i;
   for (i = 0; i < (int)nvbr->number_of_params(); i++)
   {
      CGtype type;
      CGresource res;
      CGenum var;
      int rin;
      const char *name;
      std::vector<float> dv;
      std::vector<unsigned int> ec;
      const char *sem;
      CGenum dir;
      int no;
      bool is_referenced;
      bool is_shared;
      nvbr->get_param( i, type, res, var, rin, &name, dv, ec, &sem, dir, no, is_referenced, is_shared );

      //This code is there twice,
      //here it fixes a sce-cgc "bug" or weak feature, the user has asked for a shared parameter, but the flag is not set
      //it happens for unreferenced item in a structure
      //it will break if we support unshared with the semantic 'C###'
      //later it will be there to fix the contiguity for arrays and the "share" part, but it's actually 2 different problems.
      if (strlen(sem)>=2 && sem[0] == 'C')
      {
         const char *szSem = sem+1;
         //check if we have a number
         while (*szSem != '\0')
         {
            if ( (*szSem) < '0' || (*szSem) > '9')
               break;
            szSem++;
         }
         if (*szSem == '\0')
            is_shared = 1;
      }

      const char *parameterName = name;
      const char *structureEnd = NULL;
      CgStructureType *container = &root;
      int done = 0;
      const char * structureStart = parameterName;
      while (!done && *structureStart)
      {
         // process the next structural element
         structureEnd = strpbrk(structureStart, ".[");

         if (structureEnd)
         {
            //there's a special case for sampler, we don't support arrays of sampler, so unrolled the array here
            //and don't even register the structure as being an array
            if (*structureEnd == '[' && type >= CG_SAMPLER1D && type <= CG_SAMPLERCUBE)
            {
               //do we have an array of samplers and are we at the end ?
               const char *closed = strchr(structureEnd, ']');
               //RGL_ASSERT2(closed,("name error for parameter %s",parameterName));
               const char *somethingElse = strpbrk(closed, ".[");
               if (!somethingElse)
                  structureEnd = NULL; //so we are at the end we are in an array of samplers
            }
         }

         // test if we are done finding structural information
         if (structureEnd == NULL)
         {
            //set structureEnd correctly so that the rest of the function performs correctly
            structureEnd = structureStart + strlen(structureStart);
            //set the done flag to exit the loop
            done = 1;
         }

         //extract the current structural element name
         char structName[256];
         int length = (int)(structureEnd - structureStart);
         strncpy(structName, structureStart, length);
         structName[length] = '\0';

         //do we already have it in the structure ?
         CgStructureType::CgStructuralElement *structuralElement = NULL;
         int j=0;
         int elementCount = (int)container->_elements.size();
         for (j=0;j<elementCount;j++)
         {
            structuralElement = &container->_elements[j];
            if (!strcmp(structuralElement->_name,structName))
            {
               //same name, we need to check that this is the same scope ( program versus global )
               if ( (no == -1 && (structuralElement->_flags & CGPF_GLOBAL)) ||
                     (no != -1 && !(structuralElement->_flags & CGPF_GLOBAL)))
                  break;
            }
         }
         if (j==elementCount)
         {
            if (container == &root)
            {
               if (rootChildIndex != j)
               {
                  rootChildDefaultValues = false;
                  rootChildIndex = j;
               }
            }

            //we don't have it yet, add it
            container->_elements.resize(elementCount+1);
            structuralElement = &container->_elements[elementCount];
            strncpy(structuralElement->_name,structName,sizeof(structuralElement->_name));
            structuralElement->_name[sizeof(structuralElement->_name)-1] = '\0';
            structuralElement->_flags = getFlags(var,dir,no,is_referenced,is_shared,paramIndex);
            int dimensionCount = 0;

            if (strncmp(sem, "COLOR", 5) == 0 || strncmp(sem, "NORMAL", 6) == 0)
               structuralElement->_flags |= CGP_NORMALIZE;


            //check if we have a structure or an array of structure or an array of basetypes
            //then:
            // - create an array type if we have an array
            // - update the container if we have a structure or an array of structure
            int isStructure = (*structureEnd == '.');
            if (*structureEnd == '[')
            {
               dimensionCount++;
               const char *arrayEnd = strchr(structureEnd,']')+1;
               while (*arrayEnd == '[')
               {
                  arrayEnd = strchr(arrayEnd,']')+1;
                  dimensionCount++;
               }
               if (*arrayEnd == '.')
                  isStructure = true;
            }

            //we have an array, create an array type
            if (dimensionCount)
            {
               CgArrayType *arrayType = new CgArrayType;
               arrayType->_dimensionCount = dimensionCount;
               arrayType->_dimensionItemCountsOffset = (unsigned short)containers._dimensions.size();
               //initialize the dimension count;
               int k;
               for (k=0;k<dimensionCount;k++)
                  containers._dimensions.push_back(CNV2END((short)0));
               structuralElement->_type = arrayType;
               if (isStructure)
               {
                  //we have a structure inside the array
                  container = new CgStructureType;
                  container->_insideArray = true;
                  arrayType->_elementType = container;
               }
               else
               {
                  //we have a base type inside the array
                  arrayType->_elementType = new CgBaseType;
                  arrayType->_elementType->_type = type - CG_TYPE_START_ENUM;
               }
               arrayType->_elementType->_resource = res;
               arrayType->_elementType->_resourceIndex = -1;
               //special case here for shared vertex array ??? I consider them contiguous and referenced
               //I need to detect this case and it's complex
               if (bIsVertexProgram && strlen(sem)>=2 && sem[0] == 'C')
               {
                  //I have to parse the semantic !!!! that's the only info I have at that level from sce-cgc...
                  //TODO: it's going to break if we support unshared with the semantic 'C###'
                  const char *szSem = sem+1;
                  //check if we have a number
                  while (*szSem != '\0')
                  {
                     if ( (*szSem) < '0' || (*szSem) > '9')
                        break;
                     szSem++;
                  }
                  if (*szSem == '\0')
                  {
                     //fix the is_shared variable.... sce-cgc doesn't set it that's why I am doing this parsing
                     is_shared = 1;
                     int registerIndex = atoi(sem+1);
                     structuralElement->_flags |= CGP_CONTIGUOUS;
                     structuralElement->_flags |= CGPF_SHARED;
                     structuralElement->_type->_resourceIndex = registerIndex;
                  }
                  else
                     structuralElement->_type->_resourceIndex = (int)containers._resources.size();
               }
               else
                  structuralElement->_type->_resourceIndex = (int)containers._resources.size();

               structuralElement->_type->_resource = res;
            }
            else
            {
               //we have a structure
               if (isStructure)
               {
                  bool insideArray = container->_insideArray;
                  container = new CgStructureType;
                  container->_insideArray = insideArray;
                  structuralElement->_type = container;
               }
               else
               {
                  //assign the basetype and resource
                  structuralElement->_type = new CgBaseType;
                  structuralElement->_type->_type = type - CG_TYPE_START_ENUM;
                  structuralElement->_type->_resource = res;
                  if (classnames[structuralElement->_type->_type-1] == CG_PARAMETERCLASS_MATRIX)
                  {
                     //TODO, Cedric: for vertexprogram assume the matrices are always fully allocated and that they have consecuttive assignements
                     if (bIsVertexProgram)
                     {
                        structuralElement->_type->_resourceIndex = (short)rin;
                     }
                     else
                        structuralElement->_type->_resourceIndex = (int)containers._resources.size();
                  }
                  else
                  {
                     if (!container->_insideArray)
                     {
                        //vertex program
                        if (bIsVertexProgram)
                           structuralElement->_type->_resourceIndex = rin;
                        else
                        {
                           //fragment program
                           if (structuralElement->_flags & CGPV_VARYING)
                              structuralElement->_type->_resourceIndex = -1;
                           else
                           {
                              structuralElement->_type->_resourceIndex = (int)containers._resources.size();
                              containers._resources.push_back(CNV2END((unsigned short)rin));
                              int size = (int)ec.size();
                              containers._resources.push_back(CNV2END((unsigned short)size));
                              int k;
                              for (k=0;k<size;k++)
                                 containers._resources.push_back(CNV2END((unsigned short)ec[k]));
                           }
                        }
                     }
                     else
                     {
                        //will be done later
                        structuralElement->_type->_resourceIndex = (short)-1;
                        structuralElement->_type->_resource = (unsigned short)res;
                     }
                  }
               }
            }
         }
         else
         {
            if (structuralElement->_type->_type ==  CG_STRUCT+128)
            {
               container = (CgStructureType*)structuralElement->_type;
            }
            else if (structuralElement->_type->_type ==  CG_ARRAY+128)
            {
               CgArrayType *arrayType = (CgArrayType *)structuralElement->_type;
               if (arrayType->_elementType->_type >128 )
               {
                  if (arrayType->_elementType->_type != CG_STRUCT+128) //we can't have arrays of arrays
                  {
                     //RGL_ASSERT2(0,("arrays of arrays not supported"));
                  }
                  container = (CgStructureType*)arrayType->_elementType;
               }
            }
         }

         //default values should only be at the root of the hierarchical parameter structure
         //RGL_ASSERT2(!rootChildDefaultValues || dv.size(),("all the parameter below a root child should have a default value"));
         //if (dv.size() && !rootChildDefaultValues)
         if (dv.size())
         {
            int  size = (int)containers._defaultValuesIndices.size();
            if (!size  || (containers._defaultValuesIndices[size-2] != CNV2END((unsigned short)(rootChildIndex))))
            {
               //assert for default values on structures:
               //now supported:
               /*RGL_ASSERT2(structuralElement->_type->_type != CG_STRUCT + 128,("default values on structure not yet supported"));
                 RGL_ASSERT2(structuralElement->_type->_type != CG_ARRAY + 128 ||
                 ((CgArrayType *)structuralElement->_type)->_elementType->_type != CG_STRUCT + 128,
                 ("default values on arrays of structures not yet supported"));*/

               //push the index of the parameter in the global structure
               containers._defaultValuesIndices.push_back(CNV2END((unsigned short)(rootChildIndex)));
               //push the index of the default value ( could add the size if necessary )
               containers._defaultValuesIndices.push_back(CNV2END((unsigned short)defaultValues.size()));
            }
         }

         //this case is for when we have item within a struct which is in an array, we don't duplicate the struct
         //but we still need to save the resources at the array level, so with this code when we reach the end of the parameter
         //name, we only need to save the resources
         if (container->_insideArray && done)
         {
            //the reason of the reset of the flag is because the first item of the array might unreferenced
            //so it doesn't has the correct resource type
            if (is_referenced)
            {
               bool sharedContiguous = (structuralElement->_flags & CGPF_SHARED) && (structuralElement->_flags & CGP_CONTIGUOUS);
               structuralElement->_flags = getFlags(var,dir,no,is_referenced,is_shared,paramIndex);
               //special case here for shared vertex array ??? I consider them contiguous and referenced
               if (sharedContiguous) //reset the contiguous flag too then
                  structuralElement->_flags |= ( CGP_CONTIGUOUS | CGPF_SHARED);
            }
            if (bIsVertexProgram)
            {
               //special case here for shared vertex array ??? I consider them contiguous and referenced
               if (!is_shared) //if shared it's contiguous and we don't have to do that
                  containers._resources.push_back(CNV2END((unsigned short)rin));
            }
            else
            {
               //fragment program
               if (structuralElement->_flags & CGPV_VARYING)
                  containers._resources.push_back(CNV2END((unsigned short)rin));
               else
               {
                  containers._resources.push_back(CNV2END((unsigned short)rin));
                  int size = (int)ec.size();
                  containers._resources.push_back(CNV2END((unsigned short)size));
                  int k;
                  for (k=0;k<size;k++)
                     containers._resources.push_back(CNV2END((unsigned short)ec[k]));
               }
            }
         }

         //if we have an array we need to update the item counts and skip the array for the parsing of the name of the structure
         CgArrayType *arrayType = NULL;
         if (*structureEnd == '[')
         {
            int arrayCellIndex = 0;
            const char *arrayStart = structureEnd;
            const char *arrayEnd = structureEnd;
            CgBaseType *itemType = structuralElement->_type;
            if (itemType->_type >= 128)
            {
               arrayType = (CgArrayType *)itemType;
               int dimensionCount = 0;
               while (*arrayStart == '[' && dimensionCount<arrayType->_dimensionCount)
               {
                  arrayEnd = strchr(arrayStart+1,']');
                  int length =(int)(arrayEnd - arrayStart - 1);
                  char indexString[16];
                  strncpy(indexString,arrayStart+1,length);
                  indexString[length] = '\0';
                  int index = atoi(indexString);
                  int dim = CNV2END(containers._dimensions[arrayType->_dimensionItemCountsOffset + dimensionCount]);
                  if ((index+1) > dim)
                     containers._dimensions[arrayType->_dimensionItemCountsOffset + dimensionCount] = CNV2END((short)(index+1));
                  arrayCellIndex += index*getSizeofSubArray(containers,arrayType->_dimensionItemCountsOffset + dimensionCount,arrayType->_dimensionCount - dimensionCount -1,endianness);
                  arrayStart = arrayEnd+1;
                  dimensionCount++;
               }
               structureEnd = arrayStart;

               itemType = arrayType->_elementType;
               if (itemType->_type<128)
               {
                  int rowCount = rows[itemType->_type-1];
                  if (!rowCount) //no matrix here
                  {
                     //special case here for shared vertex array ??? I consider them contiguous and referenced
                     //so I add this if and don't add the resource if we have a shared here
                     //this is part of a big workaround around sce_cgc ...
                     bool sharedContiguous = (structuralElement->_flags & CGPF_SHARED) && (structuralElement->_flags & CGP_CONTIGUOUS);
                     if (!bIsVertexProgram || !sharedContiguous)
                     {
                        //add the resource index ... scary
                        containers._resources.push_back(CNV2END((unsigned short)rin));
                     }

                     //fragment program
                     if (!bIsVertexProgram)
                     {
                        int size = (int)ec.size();
                        containers._resources.push_back(CNV2END((unsigned short)size));
                        int k;
                        for (k=0;k<size;k++)
                           containers._resources.push_back(CNV2END((unsigned short)ec[k]));
                     }
                     //RGL_ASSERT2(*arrayStart == '\0',("internal error: matrix type expected"));
                     done = 1;
                  }
               }

               if (*arrayStart == '\0')
               {
                  done = 1;
               }
            }

            //if we still have some [ , it means we have a matrix
            if (*structureEnd == '[')
            {
               int dimensionCount = 0;
               while (*arrayStart == '[')
               {
                  arrayEnd = strchr(arrayStart+1,']');
                  if (itemType->_type <128)//matrix
                  {
                     if (structuralElement != current)
                     {
                        embeddedConstants = 0;
                        current = structuralElement;
                     }

                     int length =(int)(arrayEnd - arrayStart - 1);
                     char indexString[16];
                     strncpy(indexString,arrayStart+1,length);
                     indexString[length] = '\0';

                     //TODO: Cedric: for vertex program and matrices not in array, I am not adding any resource:
                     if (bIsVertexProgram)
                     {
                        if (arrayType)
                        {
                           //special case here for shared vertex array ??? I consider them contiguous and referenced
                           //so I add this if and don't add the resource if we have a shared here
                           //this is part of a big workaround around sce_cgc ...
                           bool sharedContiguous = (structuralElement->_flags & CGPF_SHARED) && (structuralElement->_flags & CGP_CONTIGUOUS);
                           if (!sharedContiguous)
                           {
                              //RGL_ASSERT2((int)containers._resources.size() == structuralElement->_type->_resourceIndex + arrayCellIndex*rows[itemType->_type-1] + embeddedConstants + index,("matrix index mismatch. some indices have been skipped"));
                              //add the resource index ... scary
                              containers._resources.push_back(CNV2END((unsigned short)rin));
                           }
                        }
                     }
                     else
                     {
                        containers._resources.push_back(CNV2END((unsigned short)rin));
                        int size = (int)ec.size();
                        containers._resources.push_back(CNV2END((unsigned short)size));
                        int k;
                        for (k=0;k<size;k++)
                           containers._resources.push_back(CNV2END((unsigned short)ec[k]));
                        embeddedConstants += k+1;
                     }
                     //RGL_ASSERT2(*(arrayEnd+1) == '\0',("internal error: matrix type expected, the parameter specifier is longer than expected"));
                     done = 1;
                  }
                  arrayStart = arrayEnd+1;
                  dimensionCount++;
               }
               structureEnd = arrayEnd;
            }

            // matrix single or array or array of matrix: the flags should be the same for all the referenced items
            if (is_referenced)
            {
               bool sharedContiguous = (structuralElement->_flags & CGPF_SHARED) && (structuralElement->_flags & CGP_CONTIGUOUS);

               unsigned short flag = getFlags(var,dir,no,is_referenced,is_shared,paramIndex);
               structuralElement->_flags = flag;

               //special case here for shared vertex array ??? I consider them contiguous and referenced
               if (sharedContiguous) //reset the contiguous flag too then
                  structuralElement->_flags |= ( CGP_CONTIGUOUS | CGPF_SHARED);

               structuralElement->_type->_resource = res;
               //if array
               if (arrayType)
                  arrayType->_elementType->_resource = res;
            }
         }


         if (done && dv.size())
         {
            //very difficult to check that there is no mistake done... should do a diff with
            //containers._defaultValuesIndices[containers._defaultValuesIndices.size()-1] and the current parameter position in
            //the structure
            for ( int jj = 0; jj < (int)dv.size(); ++jj )
               defaultValues.push_back(dv[jj]);
         }

         if (done)
         {
            if (strlen(sem))
               strncpy(structuralElement->_semantic,sem,sizeof(structuralElement->_semantic));
            else
               structuralElement->_semantic[0] = '\0';
         }
         structureStart = structureEnd+1;
      }
   }

   //we are done with the reader
   nvbr->release();
   //release singleton
   bin_io::delete_instance();

   //print the structure that we have extracted

   //transform the extracted structure into the file format
   std::vector<CgParameterEntry> parameterEntries;
   std::vector<char> parameterResources;
   fillStructureItems(containers,&root,endianness,parameterEntries,parameterResources,stringTable);

   //save it
   CgParameterTableHeader header;
   memset(&header,0,sizeof(CgParameterTableHeader));
   header.entryCount = (unsigned short)parameterEntries.size();
   header.resourceTableOffset = sizeof(CgParameterTableHeader) + (unsigned short)(parameterEntries.size()*sizeof(parameterEntries[0]) + parameterResources.size()*sizeof(parameterResources[0]));
   header.defaultValueIndexTableOffset = header.resourceTableOffset + (unsigned short)containers._resources.size() * sizeof(containers._resources[0]);
   header.defaultValueIndexCount = (unsigned short)containers._elfDefaultsIndices.size()/2;
   header.semanticIndexTableOffset = header.defaultValueIndexTableOffset + (unsigned short)containers._elfDefaultsIndices.size() * sizeof (containers._elfDefaultsIndices[0]);
   header.semanticIndexCount = (unsigned short)containers._semanticIndices.size();

   header.entryCount = CNV2END(header.entryCount);
   header.resourceTableOffset = CNV2END(header.resourceTableOffset);
   header.defaultValueIndexTableOffset = CNV2END(header.defaultValueIndexTableOffset);
   header.defaultValueIndexCount = CNV2END(header.defaultValueIndexCount);
   header.semanticIndexTableOffset = CNV2END(header.semanticIndexTableOffset);
   header.semanticIndexCount = CNV2END(header.semanticIndexCount);

   //fill the parameter section
   size_t parameterTableSize = sizeof(CgParameterTableHeader);
   parameterTableSize += (unsigned int)parameterEntries.size() * sizeof(parameterEntries[0]);
   parameterTableSize += (unsigned int)parameterResources.size() * sizeof(parameterResources[0]);
   parameterTableSize += (unsigned int)containers._resources.size() * sizeof(containers._resources[0]);
   //parameterTableSize += (unsigned int)containers._defaultValuesIndices.size() * sizeof(containers._defaultValuesIndices[0]);
   parameterTableSize += (unsigned int)containers._elfDefaultsIndices.size() * sizeof(containers._elfDefaultsIndices[0]);
   parameterTableSize += (unsigned int)containers._semanticIndices.size() * sizeof(containers._semanticIndices[0]);

   //Allocate the binary file
   int ucodeOffset = (((sizeof(CgProgramHeader)-1)/16)+1)*16;
   size_t programSize = ucodeOffset + ucodeSize + parameterTableSize;
   char *program = new char[programSize];

   //header
   memcpy(program,&cgShader,sizeof(CgProgramHeader));
   if (ucodeOffset-sizeof(CgProgramHeader))
      memset(program+sizeof(CgProgramHeader),0,ucodeOffset-sizeof(CgProgramHeader));

   //ucode
   memcpy(program + ucodeOffset,ucode,ucodeSize);
   //we can delete buffer ( memory block pointed by ucode )
   if (!bIsVertexProgram && doSwap)
      delete[] buffer;
   // !!!xxx this is to workaround what appears to be a linux platform specific bug
   // that manifests as a memory overwrite in properly allocated memory during a std::vector resize
   else
      delete[] buffer;
   // end workaround

   //parameters
   char *parameterOffset = program + ucodeOffset + ucodeSize;

   memcpy(parameterOffset,&header,sizeof(CgParameterTableHeader));
   parameterOffset += sizeof(CgParameterTableHeader);
   array_push(parameterOffset, parameterEntries);
   array_push(parameterOffset, parameterResources);
   array_push(parameterOffset, containers._resources);
   //TODO: Tmp: patch the default Value indices, no sharing yet
   /*unsigned short offset = (unsigned short)constTableOffset;
     for (int i=0;i<(int)containers._defaultValuesIndices.size();i+=2)
     {
     unsigned short value = CNV2END(containers._defaultValuesIndices[i+1]);
     containers._defaultValuesIndices[i+1] = CNV2END((unsigned short)(value + offset));
     }*/
   //array_push(parameterOffset, containers._defaultValuesIndices);
   array_push(parameterOffset, containers._elfDefaultsIndices);
   array_push(parameterOffset, containers._semanticIndices);

   //RGL_ASSERT2(parameterOffset == program + programSize,("error\n"));
   //set the return values
   *binarySize = (int)programSize;
   *binaryShader = program;

   return 0;
}

int convertNvToElfFreeBinaryShader(void *binaryShader)
{
   char *program = (char *)binaryShader;
   delete[] program;
   return 0;
}

static void pushbackUnsignedShort(std::vector<char> &parameterResources, unsigned short value)
{
   size_t size = parameterResources.size();
   parameterResources.resize(size + 2);
   *((unsigned short*)&parameterResources[size]) = value;
}

//This function fill the parameter tables that will be written to the files
static void fillStructureItems(_CGNVCONTAINERS &containers, CgStructureType *structure, int endianness,
      std::vector<CgParameterEntry> &parameterEntries,std::vector<char> &parameterResources,
      std::vector<char> &stringTable, unsigned short *arrayResourceIndex, unsigned short *arrayDefaultValueIndex)
{
   unsigned char elfEndianness = endianness; //used in the macro CNVEND

   int currentDefaultIndex = 0;
   int count = (int)structure->_elements.size();
   int i;
   for (i=0;i<count;i++)
   {
      CgStructureType::CgStructuralElement *structuralElement = &structure->_elements[i];
      size_t size = parameterEntries.size();
      parameterEntries.resize(size+1);
      CgParameterEntry *parameterEntry = &parameterEntries[size];
      parameterEntry->nameOffset = CNV2END((int)stringTableAddUnique(stringTable, structuralElement->_name));
      if (structuralElement->_semantic[0])
      {
         CgParameterSemantic semantic;
         semantic.entryIndex = CNV2END((unsigned short)size);
         semantic.reserved = 0;
         semantic.semanticOffset = CNV2END((int)stringTableAddUnique(stringTable, structuralElement->_semantic));
         containers._semanticIndices.push_back(semantic);
      }
      parameterEntry->flags = CNV2END(structuralElement->_flags);
      //prepare the typeIndex
      unsigned short typeIndex = ((unsigned short)parameterResources.size());
      //RGL_ASSERT2((typeIndex&3) == 0,("typeindex must be aligned on 4 bytes"));
      parameterEntry->typeIndex = CNV2END(typeIndex);

      CgBaseType *itemType = structuralElement->_type;
      unsigned short _resource = itemType->_resource;
      unsigned short _resourceIndex = itemType->_resourceIndex;

      //will contain the parameterEntry for this item, for simple type arrays the value will be corrected
      //parameterEntry.size() is not parameterEntryIndex in that case.
      int parameterEntryIndex;

      if (itemType->_type-128 == CG_ARRAY)
      {
         CgArrayType *arrayType = (CgArrayType *)structuralElement->_type;
         int arraySize = getSizeofSubArray(containers,arrayType->_dimensionItemCountsOffset,arrayType->_dimensionCount,endianness);
         itemType = arrayType->_elementType;
         //RGL_ASSERT2(!(itemType->_type +CG_TYPE_START_ENUM >= CG_SAMPLER1D && itemType->_type +CG_TYPE_START_ENUM<= CG_SAMPLERCUBE),("array of samplers not yet supported"));
         unsigned short arrayFlag = CGP_ARRAY;
         parameterEntry->flags |= CNV2END(arrayFlag);
         parameterResources.resize(typeIndex+sizeof(CgParameterArray));

         CgParameterArray *parameterArray = (CgParameterArray *)(&parameterResources[typeIndex]);
         if (itemType->_type-128 == CG_STRUCT )
            parameterArray->arrayType = CNV2END((unsigned short)CG_STRUCT);
         else
            parameterArray->arrayType = CNV2END(itemType->_type+128);
         parameterArray->dimensionCount = CNV2END((unsigned short)arrayType->_dimensionCount);
         int j;
         for (j=0;j<arrayType->_dimensionCount;j++)
         {
            pushbackUnsignedShort(parameterResources,(unsigned short)containers._dimensions[arrayType->_dimensionItemCountsOffset+j]); //already endian-swapped
         }
         //padding
         if (arrayType->_dimensionCount&1)
            pushbackUnsignedShort(parameterResources,CNV2END((unsigned short)0));

         //unroll the array of structure here, create one structure for each item of the array
         if (itemType->_type-128 == CG_STRUCT )
         {
            unsigned short unrolledFlag  = CGP_UNROLLED;
            parameterEntry->flags |= CNV2END(unrolledFlag);
            CgStructureType *structureType = (CgStructureType*)itemType;
            int k;

            //those lines won't work for arrays of structures containing arrays
            unsigned short _arrayResourceIndex = (unsigned short)(arrayType->_resourceIndex);
            unsigned short _arrayDefaultValueIndex = 0;

            bool hasDefaults = false;
            if (structure->_root && containers._defaultValuesIndices.size())
            {
               if (containers._defaultValuesIndices[currentDefaultIndex*2] == CNV2END((unsigned short)i))
               {
                  hasDefaults = true;
                  _arrayDefaultValueIndex = containers._defaultValuesIndices[currentDefaultIndex*2+1];
                  //containers._defaultValuesIndices[currentDefaultIndex*2] = CNV2END((unsigned short)(parameterEntries.size() - 1));
                  //currentDefaultIndex++;
               }
            }

            for (k=0;k<arraySize;k++)
            {
               size_t size = parameterEntries.size();
               parameterEntries.resize(size+1);

               CgParameterEntry &parameterArrayEntry = parameterEntries[size];

               char buffer[256];
               sprintf(buffer,"%s[%i]",structuralElement->_name,k);
               parameterArrayEntry.nameOffset = CNV2END((int)stringTableAddUnique(stringTable, buffer));
               parameterArrayEntry.flags = CNV2END(structuralElement->_flags);
               unsigned short structureFlag = CGP_STRUCTURE;
               parameterArrayEntry.flags |= CNV2END(structureFlag);

               unsigned short arrayEntryTypeIndex = (unsigned short)parameterResources.size();
               //RGL_ASSERT2((arrayEntryTypeIndex&3) == 0,("typeindex must be aligned on 4 bytes"));
               parameterResources.resize(arrayEntryTypeIndex+sizeof(CgParameterStructure));
               parameterArrayEntry.typeIndex = CNV2END(arrayEntryTypeIndex);

               CgParameterStructure *parameterStructure = (CgParameterStructure*)(&parameterResources[arrayEntryTypeIndex]);
               parameterStructure->memberCount = CNV2END((unsigned short)structureType->_elements.size());
               parameterStructure->reserved = CNV2END((unsigned short)0);

               if (hasDefaults)
                  fillStructureItems(containers,structureType,endianness,parameterEntries,parameterResources,stringTable,&_arrayResourceIndex,&_arrayDefaultValueIndex);
               else
                  fillStructureItems(containers,structureType,endianness,parameterEntries,parameterResources,stringTable,&_arrayResourceIndex);
            }

            if (hasDefaults)
            {
               //should check this value, but it's not really easy, need the entire size of the complex structure in registers
               //unsigned short ArrayBaseTypeCount = _arrayDefaultValueIndex -containers._defaultValuesIndices[currentDefaultIndex*2+1];
               currentDefaultIndex++;
            }
            //containers._defaultValuesIndices[currentDefaultIndex*2] = CNV2END((unsigned short)(parameterEntries.size() - 1));
            //currentDefaultIndex++;


            //default value expansion: we have an array of struct: we have one default value index for the array
            //since the array is unrolled we don't give any resource to the array, but instead we give the resource to each item of each structure
            //those items can be arrays or structure themselves

            //we have already output the content of the array continue to the next structure item
            continue;
         }
         else
         {
            //create the type item following the array , the type section will fill typeindex value
            size_t size = parameterEntries.size();
            parameterEntries.resize(size+1);
            parameterEntry = &parameterEntries[size];
            parameterEntry->nameOffset = CNV2END(0);
            parameterEntry->flags = CNV2END(structuralElement->_flags);

            //prepare a typeIndex for this type
            typeIndex = ((unsigned short)parameterResources.size());
            //RGL_ASSERT2((typeIndex&3) == 0,("typeindex must be aligned on 4 bytes"));
            parameterEntry->typeIndex = CNV2END(typeIndex);

            //keep the parameterEntryIndex to assign the default values to the proper item
            parameterEntryIndex = (int)size - 1;
         }
      }
      else
      {
         unsigned short contiguousFlag = CGP_CONTIGUOUS;
         parameterEntry->flags |= CNV2END(contiguousFlag);
         size_t size = parameterEntries.size();
         parameterEntryIndex = (int)size - 1;
      }

      if (itemType->_type<128)
      {
         parameterResources.resize(typeIndex+sizeof(CgParameterResource));
         CgParameterResource *parameterResource = (CgParameterResource*)(&parameterResources[typeIndex]);

         if (itemType->_type + CG_TYPE_START_ENUM == CG_BOOL) // checking _resource == CG_B only works if referenced
         {
            parameterResource->type = CNV2END((unsigned short)CGP_SCF_BOOL);
         }
         else
         {
            parameterResource->type = CNV2END((unsigned short)(itemType->_type + CG_TYPE_START_ENUM));
         }

         if ((structuralElement->_flags & CGPV_MASK) == CGPV_UNIFORM || (structuralElement->_flags & CGPV_MASK) == CGPV_CONSTANT)
         {
            //use the resource index of the array if we are in an array: pbl with arrays containing samplers...
            if (itemType->_type +CG_TYPE_START_ENUM >= CG_SAMPLER1D && itemType->_type +CG_TYPE_START_ENUM<= CG_SAMPLERCUBE)
               parameterResource->resource = CNV2END(_resource); //sampler we need the texture unit
            else
            {
               if (arrayResourceIndex)
               {
                  unsigned short tmp = *arrayResourceIndex;
                  unsigned short localflags = CNV2END(parameterEntry->flags);
                  if (!bIsVertexProgram)
                  {
                     parameterResource->resource = CNV2END(tmp);
                     int embeddedConstantCount = CNV2END(containers._resources[tmp+1]);
                     (*arrayResourceIndex) = tmp+1+1+embeddedConstantCount;
                     if (embeddedConstantCount == 0 && (CNV2END(containers._resources[tmp]) == 0))
                     {
                        //we are unrolling an array of struct, so we get individual items
                        //we should mark if they are referenced individually
                        if (parameterResource->resource == 0xffff)
                           localflags &= ~CGPF_REFERENCED;
                     }
                  }
                  else
                  {
                     //if it's not contiguous , I am not sure what to do
                     //RGL_ASSERT2(CNV2END(parameterEntry->flags) & CGP_CONTIGUOUS,("assumed parameterEntry->flags & CGP_CONTIGUOUS"));

                     if (structuralElement->_flags & CGPF_SHARED)
                     {
                        int stride = getStride(itemType);
                        parameterResource->resource = *arrayResourceIndex;
                        (*arrayResourceIndex) = tmp+stride;
                     }
                     else
                     {
                        //this is a  hack, the info will be at 2 places
                        parameterResource->resource = containers._resources[tmp];//_resources is already converted
                        (*arrayResourceIndex) = tmp+1;
                     }

                     //we are unrolling an array of struct, so we get individual items
                     //we should mark if they are referenced individually
                     if (parameterResource->resource == 0xffff)
                        localflags &= ~CGPF_REFERENCED;
                  }
                  //update the flags
                  parameterEntry->flags = CNV2END(localflags);
               }
               else
                  parameterResource->resource = CNV2END(_resourceIndex); //uniform not a sampler we need the register
            }
         }
         else
         {
            //RGL_ASSERT2((structuralElement->_flags & CGPV_MASK) == CGPV_VARYING,("assumed varying"));
            //RGL_ASSERT2(arrayResourceIndex == NULL,("varying within array of structures not yet supported"));
            parameterResource->resource = CNV2END(itemType->_resource); //we need the bind location for the varying
         }

         if (containers._defaultValuesIndices.size())
         {
            if (structure->_root)
            {
               //TODO: what about default values on structures at the root level, the structure will flatten out so the default should go on the individual items
               //but they are not at root so the indices cannot be represented right now I guess.
               //semantics and default values indices need to be updated:
               //we are trying to find the index in the sorted array of default value indices, if we are already at the end of the array we stop here
               if (currentDefaultIndex < (int)(containers._defaultValuesIndices.size()/2) && containers._defaultValuesIndices[currentDefaultIndex*2] == CNV2END((unsigned short)i))
               {
                  containers._elfDefaultsIndices.push_back(CNV2END((unsigned short)(parameterEntryIndex)));
                  containers._elfDefaultsIndices.push_back(containers._defaultValuesIndices[currentDefaultIndex*2+1]);
                  //containers._defaultValuesIndices[currentDefaultIndex*2] = CNV2END((unsigned short)(parameterEntries.size() - 1));
                  currentDefaultIndex++;
               }
            }
            else if (arrayDefaultValueIndex)
            {
               //we are in a struct or in an array of struct which has a top level default, each time we have a basic type we need to increase
               //the index

               containers._elfDefaultsIndices.push_back(CNV2END((unsigned short)(parameterEntryIndex)));
               containers._elfDefaultsIndices.push_back(*arrayDefaultValueIndex);
               //increment the default values index by the number of defaultValues slot used by this type

               int typeRegisterCount = getStride(itemType);
               *arrayDefaultValueIndex = CNV2END( (unsigned short)((CNV2END((*arrayDefaultValueIndex)))+typeRegisterCount*4));

               //no handling of the currentDefaultIndex, which is only valid at root.
            }
         }
      }
      else if (itemType->_type == CG_STRUCT + 128)
      {
         //RGL_ASSERT2(arrayResourceIndex == NULL,("struct within array of structures not yet supported"));
         unsigned short structureFlag  = CGP_STRUCTURE;
         parameterEntry->flags |= CNV2END(structureFlag);

         CgStructureType *structureType = (CgStructureType*)itemType;
         parameterResources.resize(typeIndex+sizeof(CgParameterStructure));
         CgParameterStructure *parameterStructure = (CgParameterStructure*)(&parameterResources[typeIndex]);
         parameterStructure->memberCount = CNV2END((unsigned short)structureType->_elements.size());
         parameterStructure->reserved = CNV2END((unsigned short)0);

         fillStructureItems(containers,structureType,endianness,parameterEntries,parameterResources,stringTable);

         if (containers._defaultValuesIndices.size() && structure->_root)
         {
            if (currentDefaultIndex < (int)(containers._defaultValuesIndices.size()/2) && containers._defaultValuesIndices[currentDefaultIndex*2] == CNV2END((unsigned short)i))
            {
               containers._elfDefaultsIndices.push_back(CNV2END((unsigned short)(parameterEntryIndex)));
               containers._elfDefaultsIndices.push_back(containers._defaultValuesIndices[currentDefaultIndex*2+1]);
               currentDefaultIndex++;
            }
         }
      }
   }
   //RGL_ASSERT2(!structure->_root || currentDefaultIndex == (int)containers._defaultValuesIndices.size()/2,("error\n"));
}


static int getStride(CgBaseType *type)
{
   if (type->_type <128)
   {
      if (classnames[type->_type-1] == CG_PARAMETERCLASS_MATRIX)
         return rows[type->_type-1];
      else
         return 1;
   }
   else
   {
      if (type->_type == CG_STRUCT + 128)
      {
         CgStructureType *structureType = (CgStructureType *)type;
         int res = 0;
         int i;
         int count = (int)structureType->_elements.size();
         for (i=0;i<count;i++)
            res += getStride(structureType->_elements[i]._type);
         return res;
      }
      else
      {
         //RGL_ASSERT2(0,("arrays of arrays not supported"));
         return -9999999;
      }
      //if array ??
   }
}

static int getSizeofSubArray(_CGNVCONTAINERS &containers, int dimensionIndex, int dimensionCount, int endianness)
{
   unsigned char elfEndianness = endianness; //used in the macro CNVEND
   int res = 1;
   int i;
   for ( i=0; i < dimensionCount; i++)
      res *= (int)CNV2END(containers._dimensions[dimensionIndex + i]);
   return res;
}

template<class Type> static void array_push(char* &parameterOffset, std::vector<Type> &array)
{
   size_t dataSize = array.size()*sizeof(array[0]);
   memcpy(parameterOffset,&array[0],dataSize);
   parameterOffset += dataSize;
}


unsigned short getFlags(CGenum var, CGenum dir, int no,	bool is_referenced, bool is_shared, int paramIndex)
{
   //the following is error prone, use 2 enum array to do the matching instead
   //variability
   uint16_t flags = 0;
   if (var == CG_VARYING)
      flags |= CGPV_VARYING;
   else if (var == CG_UNIFORM)
      flags |= CGPV_UNIFORM;
   else if (var == CG_CONSTANT)
      flags |= CGPV_CONSTANT;
   else if (var == CG_MIXED)
      flags |= CGPV_MIXED;

   //direction
   if (dir == CG_IN)
      flags |= CGPD_IN;
   else if (dir == CG_OUT)
      flags |= CGPD_OUT;
   else if (dir == CG_INOUT)
      flags |= CGPD_INOUT;

   //boolean
   if (is_referenced)
      flags |= CGPF_REFERENCED;
   if (is_shared)
      flags |= CGPF_SHARED;

   //is it a global parameter ?
   if (no == -1)
      flags |= CGPF_GLOBAL;
   else if (no == -2)
      flags |= CGP_INTERNAL;
   else
   {
      //RGL_ASSERT2(no>=0,("error\n"));
      /*if ( no == paramIndex ) //means that a previous parameter already had the same param index, it's an aggregated type
        flags |= CGP_AGGREGATED;*/
      paramIndex = no;
   }
   return flags;
}

/*============================================================
  CGBIO
  ============================================================ */

namespace cgc {
   namespace bio {

      bin_io* bin_io::instance_ = 0;

      bin_io::bin_io()
      {
      }

      bin_io::bin_io( const bin_io& )
      {
      }

      const bin_io*
         bin_io::instance()
         {
            if (instance_ == 0)
               instance_ = new bin_io;
            return instance_;
         }

      void
         bin_io::delete_instance()
         {
            if (instance_ != 0)
            {
               delete instance_;
               instance_ = 0;
            }
         }

      CGBIO_ERROR
         bin_io::new_elf_reader( elf_reader** obj ) const
         {
            CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;
            *obj = new elf_reader_impl;
            if ( *obj == 0 )
               ret = CGBIO_ERROR_MEMORY;
            return ret;
         }

      CGBIO_ERROR
         bin_io::new_elf_writer( elf_writer** obj ) const
         {
            CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;
            *obj = new elf_writer_impl;
            if ( *obj == 0 )
               ret = CGBIO_ERROR_MEMORY;
            return ret;
         }

      const char*
         bin_io::error_string( CGBIO_ERROR error ) const
         {
            switch ( error )
            {
               case CGBIO_ERROR_NO_ERROR:
                  return "No error";
               case CGBIO_ERROR_LOADED:
                  return "Binary file has been loaded earlier";
               case CGBIO_ERROR_FILEIO:
                  return "File input output error";
               case CGBIO_ERROR_FORMAT:
                  return "File format error";
               case CGBIO_ERROR_INDEX:
                  return "Index is out of range";
               case CGBIO_ERROR_MEMORY:
                  return "Can't allocate memory";
               case CGBIO_ERROR_RELOC:
                  return "Relocation error";
               case CGBIO_ERROR_SYMBOL:
                  return "Symbol error";
               case CGBIO_ERROR_UNKNOWN_TYPE:
                  return "Uknown type";
               default:
                  return "Unknown error";
            }
            return "Unknown error";
         }

      CGBIO_ERROR
         bin_io::new_nvb_reader( nvb_reader** obj ) const
         {
            CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;
            *obj = new nvb_reader_impl;
            if (*obj == 0)
               ret = CGBIO_ERROR_MEMORY;
            return ret;
         }

      CGBIO_ERROR
         bin_io::new_nvb_writer( nvb_writer** obj ) const
         {
            CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;
            *obj = new nvb_writer_impl;
            if (*obj == 0)
               ret = CGBIO_ERROR_MEMORY;
            return ret;
         }


   } // bio namespace
} // cgc namespace

/*============================================================
  CGBII IMPLEMENTATION
  ============================================================ */

using std::fill_n;

namespace cgc {
   namespace bio {

      elf_reader_impl::elf_reader_impl()
      {
         ref_count_ = 1;
         initialized_ = false;
         fill_n( reinterpret_cast<char*>( &header_ ), sizeof( header_ ), '\0' );
      }

      elf_reader_impl::~elf_reader_impl()
      {
         if (initialized_)
         {
            // NYI
         }
      }

      CGBIO_ERROR
         elf_reader_impl::load( const char* filename )
         {
            // NYI
            return CGBIO_ERROR_NO_ERROR;
         }

      CGBIO_ERROR
         elf_reader_impl::load( std::istream* stream, int start )
         {
            // NYI
            return CGBIO_ERROR_NO_ERROR;
         }

      bool
         elf_reader_impl::is_initialized() const
         {
            return initialized_;
         }

      ptrdiff_t
         elf_reader_impl::reference() const
         {
            return ++ref_count_;
         }

      ptrdiff_t
         elf_reader_impl::release() const
         {
            ptrdiff_t ret = --ref_count_;
            if (ref_count_ == 0)
               delete this;

            return ret;
         }

   } // bio namespace
} // cgc namespace

/*============================================================
  CGBO IMPLEMENTATION
  ============================================================ */

namespace cgc {
   namespace bio {

      elf_writer_impl::elf_writer_impl()
      {
         ref_count_ = 1;
         std::fill_n( reinterpret_cast<char*>( &header_ ), sizeof( header_ ), '\0' );
      }

      elf_writer_impl::~elf_writer_impl()
      {
         std::vector<osection_impl*>::iterator it;
         for ( it = sections_.begin(); it != sections_.end(); ++it)
         {
            delete (*it);
         }
      }

      ptrdiff_t
         elf_writer_impl::reference()
         {
            return ++ref_count_;
         }

      ptrdiff_t
         elf_writer_impl::release()
         {
            ptrdiff_t ret = --ref_count_;
            if (ref_count_ == 0)
               delete this;
            return ret;
         }

      CGBIO_ERROR
         elf_writer_impl::save( ofstream& ofs )
         {
            CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;
            if ( !ofs )
            {
               return CGBIO_ERROR_FILEIO;
            }
            header_.e_shoff = convert_endianness( (uint32_t) sizeof( Elf32_Ehdr ), endianness() );
            header_.e_phnum = convert_endianness( (uint16_t)0, endianness() );
            header_.e_shnum = convert_endianness( (uint16_t)num_of_sections(), endianness() );

            streampos header  = convert_endianness( header_.e_shoff, endianness() );
            for ( unsigned int ii = 0; ii < sections_.size(); ++ii )
            {
               sections_[ii]->save( ofs, header, section_offset( ii ) );
               header += sizeof( Elf32_Shdr );
            }

            ofs.seekp( 0 );
            ofs.write( reinterpret_cast<const char*>( &header_ ), sizeof( Elf32_Ehdr ) );
            ofs.close();

            return ret;
         }

      CGBIO_ERROR
         elf_writer_impl::save( const char* filename )
         {
            ofstream ofs( filename, std::ios::out | std::ios::binary );
            return save( ofs );
         }

      CGBIO_ERROR
         elf_writer_impl::set_attr( const unsigned char file_class,
               const unsigned char endianness,
               const unsigned char ELFversion,
               const unsigned char abi,
               const unsigned char ABIversion,
               const uint16_t type,
               const uint16_t machine,
               const uint32_t version,
               const uint32_t flags )
         {
            header_.e_ident[EI_MAG0]    = ELFMAG0;
            header_.e_ident[EI_MAG1]    = ELFMAG1;
            header_.e_ident[EI_MAG2]    = ELFMAG2;
            header_.e_ident[EI_MAG3]    = ELFMAG3;
            header_.e_ident[EI_CLASS]   = file_class;
            header_.e_ident[EI_DATA]    = endianness;
            header_.e_ident[EI_VERSION] = ELFversion;
            header_.e_ident[EI_OSABI]	= abi;
            header_.e_ident[EI_ABIVERSION]	= ABIversion;

            header_.e_type    = convert_endianness( type,    endianness );
            header_.e_machine = convert_endianness( machine, endianness );
            header_.e_version = convert_endianness( version, endianness );
            header_.e_flags   = convert_endianness( flags,   endianness );

            header_.e_ehsize    = convert_endianness( (uint16_t)(sizeof( header_ )),    endianness );
            header_.e_phentsize = convert_endianness( (uint16_t)(sizeof( Elf32_Phdr )), endianness );
            header_.e_shentsize = convert_endianness( (uint16_t)(sizeof( Elf32_Shdr )), endianness );
            header_.e_shstrndx  = convert_endianness( (uint16_t)1, endianness );

            // Create empty and section header string table sections
            osection_impl* sec0 = new osection_impl( 0, this, "", 0, 0, 0, 0, 0 );
            sections_.push_back( sec0 );
            sec0->set_name_index( 0 );


            osection_impl* shstrtab = new osection_impl( 1, this, ".shstrtab", SHT_STRTAB, 0, 0, 0, 0 );
            sections_.push_back( shstrtab );

            // Add the name to the section header string table
            ostring_table* strtbl = 0;
            void* vp = strtbl;
            if ( CGBIO_ERROR_NO_ERROR == new_section_out( CGBO_STR, shstrtab, &vp ) )
            {
               strtbl = static_cast<ostring_table*>( vp );
               uint32_t index = strtbl->add(shstrtab->name());
               shstrtab->set_name_index( index );
               strtbl->release();
            }
            return CGBIO_ERROR_NO_ERROR;
         }

      Elf32_Addr
         elf_writer_impl::get_entry() const
         {
            return convert_endianness( header_.e_entry, endianness() );
         }

      CGBIO_ERROR
         elf_writer_impl::set_entry( Elf32_Addr entry )
         {
            header_.e_entry = convert_endianness( entry, endianness() );
            return CGBIO_ERROR_NO_ERROR;
         }

      unsigned char
         elf_writer_impl::endianness() const
         {
            return header_.e_ident[EI_DATA];
         }

      uint16_t
         elf_writer_impl::num_of_sections() const
         {
            return (uint16_t)sections_.size();
         }

      osection*
         elf_writer_impl::get_section( uint16_t index ) const
         {
            osection* ret = 0;
            if ( index < num_of_sections() )
            {
               ret = sections_[index];
               ret->reference();
            }
            return ret;
         }

      osection*
         elf_writer_impl::get_section( const char* name ) const
         {
            osection* ret = 0;

            std::vector<osection_impl*>::const_iterator it;
            for ( it = sections_.begin(); it != sections_.end(); ++it )
            {
               if ( (*it)->name() == name )
               {
                  ret = *it;
                  ret->reference();
                  break;
               }
            }
            return ret;
         }

      osection*
         elf_writer_impl::add_section( const char* name,
               uint32_t type,
               uint32_t flags,
               uint32_t info,
               uint32_t align,
               uint32_t esize )
         {
            osection_impl* sec = new osection_impl( (uint16_t)sections_.size(), this, name, type, flags, info, align, esize );
            if (sec == 0)
               return sec;
            sec->reference();
            sections_.push_back( sec );
            osection* shstrtab = get_section( 1 );
            ostring_table* strtbl;
            void* vp;
            if ( CGBIO_ERROR_NO_ERROR == new_section_out( CGBO_STR, shstrtab, &vp ) )
            {
               strtbl = static_cast<ostring_table*>( vp );
               uint32_t index = strtbl->add( name );
               sec->set_name_index( index );
               strtbl->release();
            }
            shstrtab->release();
            return sec;
         }

      streampos
         elf_writer_impl::section_offset( uint16_t index ) const
         {
            streampos ret = sizeof( Elf32_Ehdr ) + sizeof( Elf32_Shdr ) * num_of_sections();
            uint16_t size = (uint16_t)sections_.size();
            uint32_t align = 0;
            for ( uint16_t ii = 0; ii < size && ii < index; ++ii )
            {
               if ( sections_[ii]->type() != SHT_NOBITS && sections_[ii]->type() != SHT_NULL )
               {
                  align = sections_[ii]->addralign();
                  if ( align > 1 && ret % align != 0 )
                  {
                     ret += align - ret % align;
                  }
                  ret += sections_[ii]->size();
               }
            }
            if ( sections_[index]->type() != SHT_NOBITS && sections_[index]->type() != SHT_NULL )
            {
               align = sections_[index]->addralign();
               if ( align > 1 && ret % align != 0 )
               {
                  ret += align - ret % align;
               }
            }
            return ret;
         }

      CGBIO_ERROR
         elf_writer_impl::new_section_out( PRODUCER type, osection* sec, void** obj ) const
         {
            CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;

            switch ( type )
            {
               case CGBO_STR:
                  *obj = new ostring_table_impl( const_cast<elf_writer_impl*>( this ), sec );
                  break;
               case CGBO_SYM:
                  *obj = new osymbol_table_impl( const_cast<elf_writer_impl*>( this ), sec );
                  break;
               case CGBO_REL:
                  *obj = new orelocation_table_impl( const_cast<elf_writer_impl*>( this ), sec );
                  break;
               case CGBO_PARAM:
                  *obj = new oparam_table_impl( const_cast<elf_writer_impl*>( this ), sec );
                  break;
               case CGBO_CONST:
                  *obj = new oconst_table_impl( const_cast<elf_writer_impl*>( this ), sec );
                  break;
               default:
                  ret  = CGBIO_ERROR_UNKNOWN_TYPE;
                  obj = 0;
            }
            return ret;
         }


      osection_impl::osection_impl( uint16_t index,
            elf_writer* cgbo,
            const char* name,
            uint32_t type,
            uint32_t flags,
            uint32_t info,
            uint32_t align,
            uint32_t esize ) :
         index_( index ),
         cgbo_( cgbo ),
         data_( 0 )
      {
         strncpy(name_, name,sizeof(name_));
         name_[sizeof(name_)-1] = '\0';
         std::fill_n( reinterpret_cast<char*>( &sh_ ), sizeof( sh_ ), '\0' );
         sh_.sh_type      = convert_endianness( type, cgbo_->endianness() );
         sh_.sh_flags     = convert_endianness( flags, cgbo_->endianness() );
         sh_.sh_info      = convert_endianness( info, cgbo_->endianness() );
         sh_.sh_addralign = convert_endianness( align, cgbo_->endianness() );
         sh_.sh_entsize   = convert_endianness( esize, cgbo_->endianness() );
      }

      osection_impl::~osection_impl()
      {
         delete [] data_;
      }

      ptrdiff_t
         osection_impl::reference()
         {
            return cgbo_->reference();
         }

      ptrdiff_t
         osection_impl::release()
         {
            return cgbo_->release();
         }

      CGBIO_ERROR
         osection_impl::save(ofstream& of, streampos header, streampos data)
         {
            CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;

            if (index() != 0 && SHT_NOBITS != type())
            {
               sh_.sh_offset = convert_endianness( (uint32_t)data, cgbo_->endianness() );
            }
            of.seekp( header );
            of.write( reinterpret_cast<const char*>( &sh_ ), sizeof( Elf32_Shdr ) );
            if ( SHT_NOBITS != type() )
            {
               of.seekp( data );
               of.write( get_data(), size() );
            }
            return ret;
         }

      uint16_t
         osection_impl::index() const
         {
            return index_;
         }

      const char *
         osection_impl::name() const
         {
            return name_;
         }

      uint32_t
         osection_impl::type() const
         {
            return convert_endianness( sh_.sh_type, cgbo_->endianness() );
         }

      uint32_t
         osection_impl::flags() const
         {
            return convert_endianness( sh_.sh_flags, cgbo_->endianness() );
         }

      uint32_t
         osection_impl::info() const
         {
            return convert_endianness( sh_.sh_info, cgbo_->endianness() );
         }

      uint32_t
         osection_impl::addralign() const
         {
            return convert_endianness( sh_.sh_addralign, cgbo_->endianness() );
         }

      uint32_t
         osection_impl::entsize() const
         {
            return convert_endianness( sh_.sh_entsize, cgbo_->endianness() );
         }

      uint32_t
         osection_impl::size() const
         {
            return convert_endianness( sh_.sh_size, cgbo_->endianness() );
         }

      uint32_t
         osection_impl::get_name_index() const
         {
            return convert_endianness( sh_.sh_name, cgbo_->endianness() );
         }

      void
         osection_impl::set_name_index( uint32_t index )
         {
            sh_.sh_name = convert_endianness( index, cgbo_->endianness() );
         }

      Elf32_Addr
         osection_impl::get_address() const
         {
            return convert_endianness( sh_.sh_addr, cgbo_->endianness() );
         }

      void
         osection_impl::set_address( Elf32_Addr addr )
         {
            sh_.sh_addr = convert_endianness( addr, cgbo_->endianness() );
         }

      uint32_t
         osection_impl::get_link() const
         {
            return convert_endianness( sh_.sh_link, cgbo_->endianness() );
         }

      void
         osection_impl::set_link( uint32_t link )
         {
            sh_.sh_link = convert_endianness( link, cgbo_->endianness() );
         }

      char*
         osection_impl::get_data() const
         {
            return data_;
         }

      CGBIO_ERROR
         osection_impl::set_data( const char* data, const uint32_t size )
         {
            CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;
            sh_.sh_size = convert_endianness( size, cgbo_->endianness() );

            if ( SHT_NOBITS == type() )
            {
               return ret;
            }
            delete [] data_;
            data_ = new char[size];
            if (size != 0)
            {
               if (data_ == 0)
               {
                  sh_.sh_size = 0;
                  ret = CGBIO_ERROR_MEMORY;
               }
               else if (data != 0)
                  std::copy( data, data + size, data_ );
            }

            return ret;
         }

      CGBIO_ERROR
         osection_impl::add_data( const char* data, const uint32_t size )
         {
            CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;

            char* new_data = new char[ osection_impl::size() + size];
            if (new_data == 0)
               ret = CGBIO_ERROR_MEMORY;
            else
            {
               std::copy( data_, data_ + osection_impl::size(), new_data );
               std::copy( data, data + size, new_data + osection_impl::size() );
               delete [] data_;
               data_ = new_data;
               sh_.sh_size = convert_endianness(osection_impl::size()+size, cgbo_->endianness() );
            }

            return ret;
         }

      ostring_table_impl::ostring_table_impl( elf_writer* cgbo, osection* section )
         : ref_count_( 1 )
           , cgbo_( cgbo )
           , section_( section )
      {
         if ( section->get_data() != 0 && section->size() != 0)
         {
            data_.insert(data_.end(), section->get_data(),section->get_data() + section->size());
            //data_.append( section->get_data(), section->size() );
         }
         cgbo_->reference();
         section_->reference();
      }

      ostring_table_impl::~ostring_table_impl()
      {
      }

      ptrdiff_t
         ostring_table_impl::reference()
         {
            cgbo_->reference();
            section_->reference();
            return ++ref_count_;
         }

      ptrdiff_t
         ostring_table_impl::release()
         {
            ptrdiff_t ret = --ref_count_;
            elf_writer* cgbo = cgbo_;
            osection* sec = section_;

            if (ref_count_ == 0)
            {
               section_->set_data( &data_[0], (uint32_t)data_.size() );
               delete this;
            }
            sec->release();
            cgbo->release();
            return ret;
         }

      const char*
         ostring_table_impl::get( uint32_t index ) const
         {
            if ( index < data_.size() )
            {
               const char* data = &data_[0];
               if (data != 0)
                  return data + index;
            }
            return 0;
         }

      uint32_t
         ostring_table_impl::find( const char* str  ) const
         {
            const char* data = &data_[0];
            size_t size = data_.size();
            const char *end = data + size;

            size_t length = strlen(str);
            if (length+1 > size)
               return 0;
            data += length;

            const char *p = (char*)memchr(data,'\0',end-data);
            while (p && (end-data)>0)
            {
               if (!memcmp(p - length, str, length))
               {
#ifdef MSVC
#pragma warning( push )
#pragma warning ( disable : 4311 )
#endif
                  return (uint32_t)(p - length - &data_[0]);
#ifdef MSVC
#pragma warning ( pop )
#endif
               }
               data = p+1;	
               p = (char*)memchr(data,'\0',end-data);
            }
            return 0;
         }

      uint32_t
         ostring_table_impl::addUnique( const char* str )
         {
            if ( data_.size() == 0 )
               data_.push_back('\0');
            uint32_t ret = find(str);
            if (ret == 0 && str[0] != '\0')
               ret = add(str);
            return ret;
         }

      uint32_t
         ostring_table_impl::add( const char* str )
         {
            uint32_t ret = (uint32_t)data_.size();

            if ( data_.size() == 0 )
            {
               data_.push_back('\0');
               ret = 1;
            }

            data_.insert( data_.end(), str ,str + strlen(str) + 1);

            return ret;
         }

      oconst_table_impl::oconst_table_impl( elf_writer* cgbo, osection* section )
         : ref_count_( 1 )
           , cgbo_( cgbo )
           , section_( section )
      {
         if (section->get_data() != 0 && section->size() != 0)
         {
            size_t count = section->size()/sizeof(uint32_t);
            data_.resize(count);
            memcpy(&data_[0],section->get_data(),count*sizeof(uint32_t));
         }
         cgbo_->reference();
         section_->reference();
      }

      oconst_table_impl::~oconst_table_impl()
      {
      }

      ptrdiff_t
         oconst_table_impl::reference()
         {
            cgbo_->reference();
            section_->reference();
            return ++ref_count_;
         }

      ptrdiff_t
         oconst_table_impl::release()
         {
            ptrdiff_t ret = --ref_count_;
            elf_writer* cgbo = cgbo_;
            osection* sec = section_;

            if (ref_count_ == 0)
            {
               section_->set_data( (const char*)&data_[0], (uint32_t)(data_.size()*sizeof(uint32_t)) );
               delete this;
            }
            sec->release();
            cgbo->release();
            return ret;
         }

      const uint32_t*
         oconst_table_impl::get( uint32_t index ) const
         {
            if ( index < data_.size() )
            {
               return &data_[index];
            }
            return NULL;
         }

      uint32_t
         oconst_table_impl::add( const uint32_t *data, uint32_t count)
         {
            uint32_t ret = (uint32_t)data_.size();
            data_.resize(ret+count);
            memcpy(&data_[ret],data,count*sizeof(uint32_t));
            return ret;
         }

      osymbol_table_impl::osymbol_table_impl( elf_writer* cgbo, osection* sec ) :
         ref_count_( 1 ), cgbo_( cgbo ), section_( sec )
      {
         cgbo_->reference();
         section_->reference();
         if (section_->size() == 0)
         {
            Elf32_Sym entry;
            entry.st_name  = 0;
            entry.st_value = 0;
            entry.st_size  = 0;
            entry.st_info  = 0;
            entry.st_other = 0;
            entry.st_shndx = 0;
            CGBIO_ERROR err = section_->add_data( reinterpret_cast<char*>( &entry ), sizeof( entry ) );
            if ( CGBIO_ERROR_NO_ERROR != err )
            {
               ;
            }
         }
      }

      osymbol_table_impl::~osymbol_table_impl()
      {
      }

      ptrdiff_t
         osymbol_table_impl::reference()
         {
            cgbo_->reference();
            section_->reference();
            return ++ref_count_;
         }

      ptrdiff_t
         osymbol_table_impl::release()
         {
            ptrdiff_t ret = --ref_count_;
            elf_writer* cgbo = cgbo_;
            osection* sec = section_;
            if (ref_count_ == 0 )
            {
               delete this;
            }
            sec->release();
            cgbo->release();
            return ret;
         }

      uint32_t
         osymbol_table_impl::add_entry( uint32_t	name,
               Elf32_Addr	value,
               uint32_t	size,
               unsigned char	info,
               unsigned char	other,
               uint16_t	shndx )
         {
            Elf32_Sym entry;
            entry.st_name  = convert_endianness( name, cgbo_->endianness() );
            entry.st_value = convert_endianness( value, cgbo_->endianness() );
            entry.st_size  = convert_endianness( size, cgbo_->endianness() );;
            entry.st_info  = info;
            entry.st_other = other;
            entry.st_shndx = convert_endianness( shndx, cgbo_->endianness() );
            CGBIO_ERROR err = section_->add_data( reinterpret_cast<char*>( &entry ), sizeof( entry ) );
            if ( CGBIO_ERROR_NO_ERROR != err )
            {
               ;
            }
            uint32_t ret = section_->size() / sizeof(Elf32_Sym) - 1;
            return ret;
         }

      uint32_t
         osymbol_table_impl::add_entry( uint32_t	name,
               Elf32_Addr	value,
               uint32_t	size,
               unsigned char	bind,
               unsigned char	type,
               unsigned char	other,
               uint16_t	shndx )
         {
            return 0;
         }

      uint32_t
         osymbol_table_impl::add_entry( ostring_table*	strtab,
               const char*	str,
               Elf32_Addr	value,
               uint32_t	size,
               unsigned char	info,
               unsigned char	other,
               uint16_t	shndx )
         {
            return 0;
         }

      uint32_t
         osymbol_table_impl::add_entry( ostring_table*	strtab,
               const char*	str,
               Elf32_Addr	value,
               uint32_t	size,
               unsigned char	bind,
               unsigned char	type,
               unsigned char	other,
               uint16_t	shndx )
         {
            return 0;
         }


      orelocation_table_impl::orelocation_table_impl( elf_writer* cgbo, osection* sec )
      {
      }

      orelocation_table_impl::~orelocation_table_impl()
      {
      }

      ptrdiff_t
         orelocation_table_impl::reference()
         {
            return 0;
         }

      ptrdiff_t
         orelocation_table_impl::release()
         {
            return 0;
         }

      CGBIO_ERROR
         orelocation_table_impl::add_entry( Elf32_Addr offset,
               uint32_t info )
         {
            return CGBIO_ERROR_NO_ERROR;
         }

      CGBIO_ERROR
         orelocation_table_impl::add_entry( Elf32_Addr offset,
               uint32_t symbol,
               unsigned char type )
         {
            return CGBIO_ERROR_NO_ERROR;
         }

      CGBIO_ERROR
         orelocation_table_impl::add_entry( Elf32_Addr offset,
               uint32_t info,
               Elf32_Sword addend )
         {
            return CGBIO_ERROR_NO_ERROR;
         }


      CGBIO_ERROR
         orelocation_table_impl::add_entry( Elf32_Addr offset,
               uint32_t symbol,
               unsigned char type,
               Elf32_Sword addend )
         {
            return CGBIO_ERROR_NO_ERROR;
         }


      CGBIO_ERROR
         orelocation_table_impl::add_entry( ostring_table* pStrWriter,
               const char* str,
               osymbol_table* pSymWriter,
               Elf32_Addr value,
               uint32_t size,
               unsigned char symInfo,
               unsigned char other,
               uint16_t shndx,
               Elf32_Addr offset,
               unsigned char type )
         {
            return CGBIO_ERROR_NO_ERROR;
         }


      CGBIO_ERROR
         orelocation_table_impl::add_entry( ostring_table* pStrWriter,
               const char* str,
               osymbol_table* pSymWriter,
               Elf32_Addr value,
               uint32_t size,
               unsigned char symInfo,
               unsigned char other,
               uint16_t shndx,
               Elf32_Addr offset,
               unsigned char type,
               Elf32_Sword addend )
         {
            return CGBIO_ERROR_NO_ERROR;
         }


      oparam_table_impl::oparam_table_impl( elf_writer* cgbo, osection* sec ) :
         ref_count_( 1 ), cgbo_( cgbo ), section_( sec )
      {
         cgbo_->reference();
         section_->reference();
      }

      oparam_table_impl::~oparam_table_impl()
      {
      }

      ptrdiff_t
         oparam_table_impl::reference()
         {
            cgbo_->reference();
            section_->reference();
            return ++ref_count_;
         }

      ptrdiff_t
         oparam_table_impl::release()
         {
            ptrdiff_t ret = --ref_count_;
            elf_writer* cgbo = cgbo_;
            osection* sec = section_;
            if (ref_count_ == 0)
               delete this;
            sec->release();
            cgbo->release();
            return ret;
         }

      CGBIO_ERROR
         oparam_table_impl::add_entry( Elf32_cgParameter& parameter )
         {
            void* vp = &parameter;
            return section_->add_data( static_cast<char*>( vp ), sizeof( parameter ) );
         }


   } // bio namespace
} // cgc namespace

/*============================================================
  NVBI IMPLEMENTATION
  ============================================================ */

namespace cgc {
   namespace bio {

      nvb_reader_impl::nvb_reader_impl()
      {
         ref_count_ = 1;
         offset_ = 0;
         image_ = 0;
         owner_ = false;
         loaded_ = false;
         endianness_ = host_endianness();
         std::fill_n( reinterpret_cast<char*>( &header_ ), sizeof( header_ ), '\0' );
      }

      nvb_reader_impl::~nvb_reader_impl()
      {
         if (image_ != 0)
            delete [] image_;
      }

      ptrdiff_t
         nvb_reader_impl::reference() const
         {
            return ++ref_count_;
         }

      ptrdiff_t
         nvb_reader_impl::release() const
         {
            ptrdiff_t ret = --ref_count_;
            if (ref_count_ == 0)
            {
               delete this;
            }
            return ret;
         }

      CGBIO_ERROR
         nvb_reader_impl::loadFromString( const char* source, size_t length)
         {
            if ( loaded_ )
               return CGBIO_ERROR_LOADED;

            CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;
            while (1)
            {
               if (length < sizeof( header_ ))
               {
                  ret = CGBIO_ERROR_FORMAT;
                  break;
               }
               memcpy(&header_ ,source,sizeof( header_ ));
               if ( CG_BINARY_FORMAT_REVISION != header_.binaryFormatRevision )
               {
                  endianness_ = ( CGBIODATALSB == endianness_ ) ? CGBIODATAMSB : CGBIODATALSB;
                  int binaryRevision = convert_endianness( header_.binaryFormatRevision, endianness_ );
                  if ( CG_BINARY_FORMAT_REVISION != binaryRevision )
                  {
                     ret = CGBIO_ERROR_FORMAT;
                     break;
                  }
               }
               size_t sz = length;
               image_ = new char[sz];
               memcpy(image_,source,sz);
               loaded_ = true;
               ret = CGBIO_ERROR_NO_ERROR;
               break;
            }
            return ret;
         }

      bool
         nvb_reader_impl::is_loaded() const
         {
            return loaded_;
         }

      unsigned char
         nvb_reader_impl::endianness() const
         {
            return endianness_;
         }

      CGprofile
         nvb_reader_impl::profile() const
         {
            return (CGprofile) convert_endianness( (unsigned int) header_.profile, endianness() );
         }

      unsigned int
         nvb_reader_impl::revision() const
         {
            return convert_endianness( header_.binaryFormatRevision, endianness() );
         }

      unsigned int
         nvb_reader_impl::size() const
         {
            return convert_endianness( header_.totalSize, endianness() );
         }

      unsigned int
         nvb_reader_impl::number_of_params() const
         {
            return convert_endianness( header_.parameterCount, endianness() );
         }

      unsigned int
         nvb_reader_impl::ucode_size() const
         {
            return convert_endianness( header_.ucodeSize, endianness() );
         }

      const char*
         nvb_reader_impl::ucode() const
         {
            if (image_ == 0 || ucode_size() == 0)
               return 0;
            return ( image_ + convert_endianness( header_.ucode, endianness() ) );
         }

      const CgBinaryFragmentProgram*
         nvb_reader_impl::fragment_program() const
         {
            if (image_ == 0)
               return 0;
            return reinterpret_cast<CgBinaryFragmentProgram*>( &image_[convert_endianness( header_.program, endianness_ )] );
         }

      const CgBinaryVertexProgram*
         nvb_reader_impl::vertex_program() const
         {
            if (image_ == 0)
               return 0;
            return reinterpret_cast<CgBinaryVertexProgram*>( &image_[convert_endianness( header_.program, endianness_ )] );
         }

      CGBIO_ERROR
         nvb_reader_impl::get_param_name( unsigned int index, const char **name, bool& is_referenced ) const
         {
            if (index >= number_of_params())
               return CGBIO_ERROR_INDEX;

            if (image_ == 0)
               return CGBIO_ERROR_NO_ERROR;

            const CgBinaryParameter* params = reinterpret_cast<CgBinaryParameter*>( &image_[convert_endianness( header_.parameterArray, endianness_ )] );
            const CgBinaryParameter& pp = params[index];
            is_referenced = convert_endianness( pp.isReferenced, endianness() ) != 0;
            CgBinaryStringOffset nm_offset = convert_endianness( pp.name,endianness() );
            if (nm_offset != 0)
               *name = &image_[nm_offset];
            else
               *name = "";
            return CGBIO_ERROR_NO_ERROR;
         }

      CGBIO_ERROR
         nvb_reader_impl::get_param( unsigned int index,
               CGtype& type,
               CGresource& resource,
               CGenum& variability,
               int& resource_index,
               const char ** name,
               std::vector<float>& default_value,
               std::vector<unsigned int>& embedded_constants,
               const char ** semantic,
               CGenum& direction,
               int& paramno,
               bool& is_referenced,
               bool& is_shared ) const
         {
            if (index >= number_of_params())
               return CGBIO_ERROR_INDEX;

            if (image_ == 0)
               return CGBIO_ERROR_NO_ERROR;

            const CgBinaryParameter* params = reinterpret_cast<CgBinaryParameter*>( &image_[convert_endianness( header_.parameterArray, endianness_ )] );
            const CgBinaryParameter& pp = params[index];
            type		= static_cast<CGtype>(		convert_endianness( static_cast<unsigned int>( pp.type ),	endianness() ) );
            resource		= static_cast<CGresource>(	convert_endianness( static_cast<unsigned int>( pp.res ),	endianness() ) );
            variability		= static_cast<CGenum>(		convert_endianness( static_cast<unsigned int>( pp.var ),	endianness() ) );
            resource_index	=				convert_endianness( pp.resIndex,				endianness() );
            direction		= static_cast<CGenum>(		convert_endianness( static_cast<unsigned int>( pp.direction ),	endianness() ) );
            paramno		=				convert_endianness( pp.paramno,					endianness() );
            is_referenced	=				convert_endianness( pp.isReferenced,				endianness() ) != 0;
            is_shared		=				convert_endianness( pp.isShared,				endianness() ) != 0;
            CgBinaryStringOffset		nm_offset =	convert_endianness( pp.name,					endianness() );
            CgBinaryFloatOffset			dv_offset =	convert_endianness( pp.defaultValue,				endianness() );
            CgBinaryEmbeddedConstantOffset	ec_offset =	convert_endianness( pp.embeddedConst,				endianness() );
            CgBinaryStringOffset		sm_offset =	convert_endianness( pp.semantic,				endianness() );
            if (nm_offset != 0)
               *name = &image_[nm_offset];
            else
               *name = "";

            if (sm_offset != 0)
               *semantic = &image_[sm_offset];
            else
               *semantic = "";
            if (dv_offset != 0)
            {
               char *vp = &image_[dv_offset];
               for (int ii = 0; ii < 4; ++ii)
               {
                  int tmp;
                  memcpy(&tmp,vp+4*ii,4);
                  tmp = convert_endianness(tmp,endianness());
                  float tmp2;
                  memcpy(&tmp2,&tmp,4);
                  default_value.push_back( tmp2 );
               }
            }
            if (ec_offset != 0)
            {
               void *vp = &image_[ec_offset];
               CgBinaryEmbeddedConstant& ec = *(static_cast<CgBinaryEmbeddedConstant*>( vp ));
               for (unsigned int ii = 0; ii < convert_endianness( ec.ucodeCount, endianness() ); ++ii)
               {
                  unsigned int offset = convert_endianness( ec.ucodeOffset[ii], endianness() );
                  embedded_constants.push_back( offset );
               }
            }
            return CGBIO_ERROR_NO_ERROR;
         }


   }
}

/*============================================================
  NVBO IMPLEMENTATION
  ============================================================ */

using std::copy;
using std::fill_n;

namespace cgc {
   namespace bio {

      nvb_writer_impl::nvb_writer_impl()
      {
         ref_count_ = 1;
         fill_n( reinterpret_cast<char*>( &header_ ), sizeof( header_ ), '\0' );
      }

      nvb_writer_impl::~nvb_writer_impl()
      {
      }

      ptrdiff_t
         nvb_writer_impl::reference()
         {
            return ++ref_count_;
         }

      ptrdiff_t
         nvb_writer_impl::release()
         {
            ptrdiff_t ret = --ref_count_;
            if (ref_count_ == 0)
               delete this;
            return ret;
         }

      CGBIO_ERROR
         nvb_writer_impl::save( ofstream& ofs )
         {
            CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;
            if ( !ofs )
            {
               return CGBIO_ERROR_FILEIO;
            }
            header_.e_shoff = convert_endianness( (uint32_t)sizeof( Elf32_Ehdr ), endianness() );
            header_.e_phnum = convert_endianness( 0, endianness() );
            ofs.seekp( 0 );
            ofs.write( reinterpret_cast<const char*>( &header_ ), sizeof( Elf32_Ehdr ) );
            ofs.close();

            return ret;
         }

      CGBIO_ERROR
         nvb_writer_impl::save( const char* filename )
         {
            ofstream ofs( filename, std::ios::out | std::ios::binary );
            return save( ofs );
         }

      CGBIO_ERROR
         nvb_writer_impl::set_attr( const unsigned char file_class,
               const unsigned char endianness,
               const unsigned char ELFversion,
               const unsigned char abi,
               const unsigned char ABIversion,
               const uint16_t type,
               const uint16_t machine,
               const uint32_t version,
               const uint32_t flags )
         {
            header_.e_ident[EI_MAG0]    = ELFMAG0;
            header_.e_ident[EI_MAG1]    = ELFMAG1;
            header_.e_ident[EI_MAG2]    = ELFMAG2;
            header_.e_ident[EI_MAG3]    = ELFMAG3;
            header_.e_ident[EI_CLASS]   = file_class;
            header_.e_ident[EI_DATA]    = endianness;
            header_.e_ident[EI_VERSION] = ELFversion;
            header_.e_ident[EI_OSABI]	= abi;
            header_.e_ident[EI_ABIVERSION]	= ABIversion;

            header_.e_type    = convert_endianness( type,    endianness );
            header_.e_machine = convert_endianness( machine, endianness );
            header_.e_version = convert_endianness( version, endianness );
            header_.e_flags   = convert_endianness( flags,   endianness );

            header_.e_ehsize    = convert_endianness( (uint16_t)sizeof( header_ ),    endianness );
            header_.e_phentsize = convert_endianness( (uint16_t)sizeof( Elf32_Phdr ), endianness );
            header_.e_shentsize = convert_endianness( (uint16_t)sizeof( Elf32_Shdr ), endianness );
            header_.e_shstrndx  = convert_endianness( 1, endianness );
            return CGBIO_ERROR_NO_ERROR;
         }

      Elf32_Addr
         nvb_writer_impl::get_entry() const
         {
            return convert_endianness( header_.e_entry, endianness() );
         }

      CGBIO_ERROR
         nvb_writer_impl::set_entry( Elf32_Addr entry )
         {
            header_.e_entry = convert_endianness( entry, endianness() );
            return CGBIO_ERROR_NO_ERROR;
         }

      unsigned char
         nvb_writer_impl::endianness() const
         {
            return header_.e_ident[EI_DATA];
         }

      oparam_array_impl::oparam_array_impl( nvb_writer* cgbo ) :
         ref_count_( 1 ), cgbo_( cgbo )
      {
         cgbo_->reference();
      }

      oparam_array_impl::~oparam_array_impl()
      {
      }

      ptrdiff_t oparam_array_impl::reference()
      {
         cgbo_->reference();
         return ++ref_count_;
      }

      ptrdiff_t
         oparam_array_impl::release()
         {
            ptrdiff_t ret = --ref_count_;
            nvb_writer* cgbo = cgbo_;

            if (ref_count_ == 0)
               delete this;

            cgbo->release();
            return ret;
         }

      CGBIO_ERROR
         oparam_array_impl::add_entry()
         {
            CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;
            return ret;
         }


   }
}
