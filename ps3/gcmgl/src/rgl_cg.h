#ifndef _RGL_CG_H
#define _RGL_CG_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <Cg/cg.h>
#include <Cg/cgGL.h>
#include <Cg/CgCommon.h>
#include <Cg/cgBinary.h>
#include <Cg/CgInternal.h>
#include <Cg/CgProgramGroup.h>
#include <Cg/cgc.h>

#include "../include/export/PSGL/psgl.h"
#include "../include/PSGL/private.h"
#include "../include/PSGL/Types.h"
#include "../include/PSGL/Utils.h"
#include "libelf/readelf.h"

#include "cg/cgbtypes.h"
#include "cg/cgnv2rt.h"

#include "cg/cgbio.hpp"
#include "cg/nvbiimpl.hpp"
#include "cg/cgbutils.hpp"
#include "cg/cgbtypes.h"

typedef struct
{
   const char* elfFile;
   size_t elfFileSize;

   const char *symtab;
   size_t symbolSize;
   size_t symbolCount;
   const char *symbolstrtab;

   const char* shadertab;
   size_t shadertabSize;
   const char* strtab;
   size_t strtabSize;
   const char* consttab;
   size_t consttabSize;
} CGELFBinary;

typedef struct
{
   const char *texttab;
   size_t texttabSize;
   const char *paramtab;
   size_t paramtabSize;
   int index;
} CGELFProgram;

extern int rglpCopyProgram (void *src_data, void *dst_data);
extern int rglpGenerateFragmentProgram (void *data,
   const CgProgramHeader *programHeader, const void *ucode,
   const CgParameterTableHeader *parameterHeader,
   const char *stringTable, const float *defaultValues );

extern int rglpGenerateVertexProgram (void *data,
   const CgProgramHeader *programHeader, const void *ucode,
   const CgParameterTableHeader *parameterHeader, const char *stringTable,
   const float *defaultValues );

extern CGprogram rglpCgUpdateProgramAtIndex( CGprogramGroup group, int index,
   int refcount );

extern void rglpProgramErase (void *data);

extern bool cgOpenElf( const void *ptr, size_t size, CGELFBinary *elfBinary );
extern bool cgGetElfProgramByIndex( CGELFBinary *elfBinary, int index, CGELFProgram *elfProgram );

extern CGprogram rglCgCreateProgram( CGcontext ctx, CGprofile profile, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader, const char *stringTable, const float *defaultValues );

#endif
