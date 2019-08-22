package org.retroarch.browser;

import org.retroarch.R;

import java.io.*;

import android.content.*;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetManager;
import android.annotation.TargetApi;
import android.app.*;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.net.Uri;
import android.os.*;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.widget.*;
import android.util.Log;
import android.view.*;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.inputmethod.*;
import android.graphics.drawable.*;

// JELLY_BEAN_MR1 = 17

class ModuleWrapper implements IconAdapterItem {
	public final File file;
	private ConfigFile config;

	public ModuleWrapper(Context aContext, File aFile, ConfigFile config) throws IOException {
		file = aFile;
		this.config = config;
	}

	@Override
	public boolean isEnabled() {
		return true;
	}

	@Override
	public String getText() {
		String stripped = file.getName().replace(".so", "");
		if (config.keyExists(stripped)) {
			return config.getString(stripped);
		} else
			return stripped;
	}

	@Override
	public int getIconResourceId() {
		return 0;
	}

	@Override
	public Drawable getIconDrawable() {
		return null;
	}
}

public class RetroArch extends Activity implements
		AdapterView.OnItemClickListener {
	private IconAdapter<ModuleWrapper> adapter;
	static private final int ACTIVITY_LOAD_ROM = 0;
	static private String libretro_path;
	static private Double report_refreshrate;
	static private final String TAG = "RetroArch-Phoenix";
	private ConfigFile config;
	private ConfigFile core_config;
	
	private final double getDisplayRefreshRate() {
		// Android is *very* likely to screw this up.
		// It is rarely a good value to use, so make sure it's not
		// completely wrong. Some phones return refresh rates that are completely bogus
		// (like 0.3 Hz, etc), so try to be very conservative here.
		final WindowManager wm = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
		final Display display = wm.getDefaultDisplay();
		double rate = display.getRefreshRate();
		if (rate > 61.0 || rate < 58.0)
			rate = 59.95;
		return rate;
	}

	private final double getRefreshRate() {
		double rate = 0;
		SharedPreferences prefs = PreferenceManager
				.getDefaultSharedPreferences(getBaseContext());
		String refresh_rate = prefs.getString("video_refresh_rate", "");
		if (!refresh_rate.isEmpty()) {
			try {
				rate = Double.parseDouble(refresh_rate);
			} catch (NumberFormatException e) {
				Log.e(TAG, "Cannot parse: " + refresh_rate + " as a double!");
				rate = getDisplayRefreshRate();
			}
		} else {
			rate = getDisplayRefreshRate();
		}
		
		Log.i(TAG, "Using refresh rate: " + rate + " Hz.");
		return rate;
	}
	
	private String readCPUInfo() {
		String result = "";

		try {
			BufferedReader br = new BufferedReader(new InputStreamReader(
					new FileInputStream("/proc/cpuinfo")));

			String line;
			while ((line = br.readLine()) != null)
				result += line + "\n";
			br.close();
		} catch (IOException ex) {
			ex.printStackTrace();
		}
		return result;
	}
	
	private boolean cpuInfoIsNeon(String info) {
		return info.contains("neon");
	}

	private byte[] loadAsset(String asset) throws IOException {
		String path = asset;
		InputStream stream = getAssets().open(path);
		int len = stream.available();
		byte[] buf = new byte[len];
		stream.read(buf, 0, len);
		return buf;
	}
	
	private void extractAssets(AssetManager manager, String dataDir, String relativePath, int level) throws IOException {
		final String[] paths = manager.list(relativePath);
		if (paths != null && paths.length > 0) { // Directory
			//Log.d(TAG, "Extracting assets directory: " + relativePath);
			for (final String path : paths)
				extractAssets(manager, dataDir, relativePath + (level > 0 ? File.separator : "") + path, level + 1);
		} else { // File, extract.
			//Log.d(TAG, "Extracting assets file: " + relativePath);
			
			String parentPath = new File(relativePath).getParent();
			if (parentPath != null) {
				File parentFile = new File(dataDir, parentPath);
				parentFile.mkdirs(); // Doesn't throw.
			}
			
			byte[] asset = loadAsset(relativePath);
			BufferedOutputStream writer = new BufferedOutputStream(
					new FileOutputStream(new File(dataDir, relativePath)));

			writer.write(asset, 0, asset.length);
			writer.flush();
			writer.close();
		}
	}
	
	private void extractAssets() {
		int version = 0;
		try {
			version = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
		} catch(NameNotFoundException e) {
			// weird exception, shouldn't happen
		}
		
		try {
			AssetManager assets = getAssets();
			String dataDir = getDataDir();
			File cacheVersion = new File(dataDir, ".cacheversion");
			if (cacheVersion != null && cacheVersion.isFile() && cacheVersion.canRead() && cacheVersion.canWrite())
			{
				DataInputStream cacheStream = new DataInputStream(new FileInputStream(cacheVersion));

				int currentCacheVersion = 0;
				try {
					currentCacheVersion = cacheStream.readInt();
				} catch (IOException e) {}
			    cacheStream.close();
			    
				if (currentCacheVersion == version)
				{
					Log.i("ASSETS", "Assets already extracted, skipping...");
					return;
				}
			}
			
			//extractAssets(assets, cacheDir, "", 0);
			Log.i("ASSETS", "Extracting shader assets now ...");
			try {
				extractAssets(assets, dataDir, "Shaders", 1);
			} catch (IOException e) {
				Log.i("ASSETS", "Failed to extract shaders ...");
			}
			
			Log.i("ASSETS", "Extracting overlay assets now ...");
			try {
				extractAssets(assets, dataDir, "Overlays", 1);
			} catch (IOException e) {
				Log.i("ASSETS", "Failed to extract overlays ...");
			}
			
			DataOutputStream outputCacheVersion = new DataOutputStream(new FileOutputStream(cacheVersion, false));
			outputCacheVersion.writeInt(version);
			outputCacheVersion.close();
		} catch (IOException e) {
			Log.e(TAG, "Failed to extract assets to cache.");			
		}
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		try {
			config = new ConfigFile(new File(getDefaultConfigPath()));
		} catch (IOException e) {
			config = new ConfigFile();
		}
		
		core_config = new ConfigFile();
		try {
			core_config.append(getAssets().open("libretro_cores.cfg"));
		} catch (IOException e) {
			Log.e(TAG, "Failed to load libretro_cores.cfg from assets.");
		}
		
		String cpuInfo = readCPUInfo();
		boolean cpuIsNeon = cpuInfoIsNeon(cpuInfo);
		report_refreshrate = getDisplayRefreshRate();
		
		// Extracting assets appears to take considerable amount of time, so
		// move extraction to a thread.
		Thread assetThread = new Thread(new Runnable() {
			public void run() {
				extractAssets();
			}
		});
		assetThread.start();
		
		setContentView(R.layout.line_list);

		// Setup the list
		adapter = new IconAdapter<ModuleWrapper>(this, R.layout.line_list_item);
		ListView list = (ListView) findViewById(R.id.list);
		list.setAdapter(adapter);
		list.setOnItemClickListener(this);

		setTitle("Select Libretro core");

		// Populate the list
		final String modulePath = getApplicationInfo().nativeLibraryDir;
		final File[] libs = new File(modulePath).listFiles();
		for (final File lib : libs) {
			String libName = lib.getName();
			
			// Never append a NEON lib if we don't have NEON.
			if (libName.contains("neon") && !cpuIsNeon)
				continue;
			
			// If we have a NEON version with NEON capable CPU,
			// never append a non-NEON version.
			if (cpuIsNeon && !libName.contains("neon")) {
				boolean hasNeonVersion = false;
				for (final File lib_ : libs) {
					String otherName = lib_.getName();
					String baseName = libName.replace(".so", "");
					if (otherName.contains("neon") && otherName.startsWith(baseName)) {
						hasNeonVersion = true;
						break;
					}
				}
				
				if (hasNeonVersion)
					continue;
			}
			
			// Allow both libretro-core.so and libretro_core.so.
			if (libName.startsWith("libretro") && !libName.startsWith("libretroarch")) {
				try {
					adapter.add(new ModuleWrapper(this, lib, core_config));
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
		}
		
		this.setVolumeControlStream(AudioManager.STREAM_MUSIC);
		
		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
			this.registerForContextMenu(findViewById(android.R.id.content));
		}
	}
	
	boolean detectDevice(boolean show_dialog)
	{
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
		SharedPreferences.Editor edit = prefs.edit();
		edit.putBoolean("video_threaded", true);
		edit.commit();
		
		Log.i("Device MODEL", android.os.Build.MODEL);
		if (android.os.Build.MODEL.equals("SHIELD"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(this)
			.setTitle("NVidia Shield detected")
			.setMessage("Would you like to set up the ideal configuration options for your device?\nNOTE: For optimal performance, turn off Google Account sync, GPS and Wifi in your Android settings menu.\n\nNOTE: If you know what you are doing, it is advised to turn 'Threaded Video' off afterwards and try running games without it. In theory it will be smoother, but your mileage varies depending on your device and configuration.")
			.setPositiveButton("Yes", new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface dialog, int which) {
					SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
					SharedPreferences.Editor edit = prefs.edit();
					edit.putString("video_refresh_rate", Double.valueOf(60.00d).toString());
					edit.putBoolean("input_overlay_enable", false);
					edit.putBoolean("input_autodetect_enable", true);
					edit.commit();
				}
			})
			.setNegativeButton("No", null);
			alert.show();
			return true;
		}
		else if (android.os.Build.ID.equals("JSS15J"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(this)
			.setTitle("Nexus 7 2013 detected")
			.setMessage("Would you like to set up the ideal configuration options for your device?\nNOTE: For optimal performance, turn off Google Account sync, GPS and Wifi in your Android settings menu. \n\nNOTE: If you know what you are doing, it is advised to turn 'Threaded Video' off afterwards and try running games without it. In theory it will be smoother, but your mileage varies depending on your device and configuration.")
			.setPositiveButton("Yes", new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface dialog, int which) {
					SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
					SharedPreferences.Editor edit = prefs.edit();
					edit.putString("video_refresh_rate", Double.valueOf(59.65).toString());
					edit.commit();
				}
			})
			.setNegativeButton("No", null);
			alert.show();
			return true;
		}
		
		if (show_dialog) {
		Toast.makeText(this,
				"Device either not detected in list or doesn't have any optimal settings in our database.",
				Toast.LENGTH_SHORT).show();
		}
		
		return false;
	}
	
	@Override
	protected void onStart() {
		super.onStart();
		
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
		
		if (!prefs.getBoolean("first_time_refreshrate_calculate", false)) {
			prefs.edit().putBoolean("first_time_refreshrate_calculate", true).commit();
			
			if (!detectDevice(false))
			{
			AlertDialog.Builder alert = new AlertDialog.Builder(this)
				.setTitle("Two modes of play - pick one")
				.setMessage("RetroArch has two modes of play: synchronize to refreshrate, and threaded video.\n\nSynchronize to refreshrate gives the most accurate results and can produce the smoothest results. However, it is hard to configure right and might result in unpleasant audio crackles when it has been configured wrong.\n\nThreaded video should work fine on most devices, but applies some adaptive video jittering to achieve this.\n\nChoose which of the two you want to use. (If you don't know, go for Threaded video). ")
				.setPositiveButton("Threaded video", new DialogInterface.OnClickListener() {
					@Override
					public void onClick(DialogInterface dialog, int which) {
						SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
						SharedPreferences.Editor edit = prefs.edit();
						edit.putBoolean("video_threaded", true);
						edit.commit();
					}
				})
				.setNegativeButton("Synchronize to refreshrate", new DialogInterface.OnClickListener() {
					@Override
					public void onClick(DialogInterface dialog, int which) {
						SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
						SharedPreferences.Editor edit = prefs.edit();
						edit.putBoolean("video_threaded", false);
						edit.commit();
						Intent i = new Intent(getBaseContext(), DisplayRefreshRateTest.class);
						startActivity(i);
					}
				});
			alert.show();
			}
		}
	}

	@Override
	public void onItemClick(AdapterView<?> aListView, View aView,
			int aPosition, long aID) {
		final ModuleWrapper item = adapter.getItem(aPosition);
		libretro_path = item.file.getAbsolutePath();

		Intent myIntent;
		myIntent = new Intent(this, ROMActivity.class);
		startActivityForResult(myIntent, ACTIVITY_LOAD_ROM);
	}
	
	private String getDataDir() {
		return getApplicationInfo().dataDir;
	}
	
	private String getDefaultConfigPath() {
		String internal = System.getenv("INTERNAL_STORAGE");
		String external = System.getenv("EXTERNAL_STORAGE");
		
		if (external != null) {
			String confPath = external + File.separator + "retroarch.cfg";
			if (new File(confPath).exists())
				return confPath;
		} else if (internal != null) {
			String confPath = internal + File.separator + "retroarch.cfg";
			if (new File(confPath).exists())
				return confPath;
		} else {
			String confPath = "/mnt/extsd/retroarch.cfg";
			if (new File(confPath).exists())
				return confPath;
		}
		
		if (internal != null && new File(internal + File.separator + "retroarch.cfg").canWrite())
			return internal + File.separator + "retroarch.cfg";
		else if (external != null && new File(internal + File.separator + "retroarch.cfg").canWrite())
			return external + File.separator + "retroarch.cfg";
		else if (getDataDir() != null)
			return getDataDir() + File.separator + "retroarch.cfg";
		else // emergency fallback, all else failed
			return "/mnt/sd/retroarch.cfg";
	}
	
	@TargetApi(17)
	private int getLowLatencyOptimalSamplingRate() {
		AudioManager manager = (AudioManager)getApplicationContext().getSystemService(Context.AUDIO_SERVICE);
		return Integer.parseInt(manager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE));
	}
	
	private int getOptimalSamplingRate() {
		int ret;
		if (android.os.Build.VERSION.SDK_INT >= 17)
			ret = getLowLatencyOptimalSamplingRate();
		else
			ret = AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_MUSIC);
		
		Log.i(TAG, "Using sampling rate: " + ret + " Hz");
		return ret;
	}
	
	private void updateConfigFile() {
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
		config.setBoolean("audio_rate_control", prefs.getBoolean("audio_rate_control", true));
		config.setInt("audio_out_rate", getOptimalSamplingRate());
		config.setBoolean("audio_enable", prefs.getBoolean("audio_enable", true));
		config.setBoolean("video_smooth", prefs.getBoolean("video_smooth", true));
		config.setBoolean("video_allow_rotate", prefs.getBoolean("video_allow_rotate", true));
		config.setBoolean("savestate_auto_load", prefs.getBoolean("savestate_auto_load", true));
		config.setBoolean("savestate_auto_save", prefs.getBoolean("savestate_auto_save", false));
		config.setBoolean("rewind_enable", prefs.getBoolean("rewind_enable", false));
		config.setBoolean("video_vsync", prefs.getBoolean("video_vsync", true));
		config.setBoolean("input_autodetect_enable", prefs.getBoolean("input_autodetect_enable", true));
		config.setBoolean("input_debug_enable", prefs.getBoolean("input_debug_enable", false));
		config.setInt("input_back_behavior", Integer.valueOf(prefs.getString("input_back_behavior", "0")));
		config.setInt("input_autodetect_icade_profile_pad1", Integer.valueOf(prefs.getString("input_autodetect_icade_profile_pad1", "0")));
		config.setInt("input_autodetect_icade_profile_pad2", Integer.valueOf(prefs.getString("input_autodetect_icade_profile_pad2", "0")));
		config.setInt("input_autodetect_icade_profile_pad3", Integer.valueOf(prefs.getString("input_autodetect_icade_profile_pad3", "0")));
		config.setInt("input_autodetect_icade_profile_pad4", Integer.valueOf(prefs.getString("input_autodetect_icade_profile_pad4", "0")));
		
		config.setDouble("video_refresh_rate", getRefreshRate());
		config.setBoolean("video_threaded", prefs.getBoolean("video_threaded", false));
		
		String aspect = prefs.getString("video_aspect_ratio", "auto");
		if (aspect.equals("full")) {
			config.setBoolean("video_force_aspect", false);
		} else if (aspect.equals("auto")) {
			config.setBoolean("video_force_aspect", true);
			config.setBoolean("video_force_aspect_auto", true);
			config.setDouble("video_aspect_ratio", -1.0);
		} else if (aspect.equals("square")) {
			config.setBoolean("video_force_aspect", true);
			config.setBoolean("video_force_aspect_auto", false);
			config.setDouble("video_aspect_ratio", -1.0);
		} else {
			double aspect_ratio = Double.parseDouble(aspect);
			config.setBoolean("video_force_aspect", true);
			config.setDouble("video_aspect_ratio", aspect_ratio);
		}
		
		config.setBoolean("video_scale_integer", prefs.getBoolean("video_scale_integer", false));
		
		String shaderPath = prefs.getString("video_shader", "");
		config.setString("video_shader", shaderPath);
		config.setBoolean("video_shader_enable",
				prefs.getBoolean("video_shader_enable", false)
						&& new File(shaderPath).exists());

		boolean useOverlay = prefs.getBoolean("input_overlay_enable", true);
		if (useOverlay) {
			String overlayPath = prefs.getString("input_overlay", getDataDir() + "/Overlays/snes-landscape.cfg");
			config.setString("input_overlay", overlayPath);
			config.setDouble("input_overlay_opacity", prefs.getFloat("input_overlay_opacity", 1.0f));
		} else {
			config.setString("input_overlay", "");
		}
		
		config.setString("savefile_directory", prefs.getBoolean("savefile_directory_enable", false) ?
				prefs.getString("savefile_directory", "") : "");
		config.setString("savestate_directory", prefs.getBoolean("savestate_directory_enable", false) ?
				prefs.getString("savestate_directory", "") : "");
		config.setString("system_directory", prefs.getBoolean("system_directory_enable", false) ?
				prefs.getString("system_directory", "") : "");
		
		config.setBoolean("video_font_enable", prefs.getBoolean("video_font_enable", true));
		
		for (int i = 1; i <= 4; i++)
		{
			final String btns[] = {"up", "down", "left", "right", "a", "b", "x", "y", "start", "select", "l", "r", "l2", "r2", "l3", "r3" };
			for (String b : btns)
			{
				String p = "input_player" + String.valueOf(i) + "_" + b + "_btn";
				config.setInt(p, prefs.getInt(p, 0));
			}
		}

		String confPath = getDefaultConfigPath();
		try {
			config.write(new File(confPath));
		} catch (IOException e) {
			Log.e(TAG, "Failed to save config file to: " + confPath);
		}
	}

	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		Intent myIntent;
		String current_ime = Settings.Secure.getString(getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
		
		updateConfigFile();
		
		switch (requestCode) {
		case ACTIVITY_LOAD_ROM:
			if (data.getStringExtra("PATH") != null) {
				Toast.makeText(this,
						"Loading: [" + data.getStringExtra("PATH") + "]...",
						Toast.LENGTH_SHORT).show();
				myIntent = new Intent(this, RetroActivity.class);
				myIntent.putExtra("ROM", data.getStringExtra("PATH"));
				myIntent.putExtra("LIBRETRO", libretro_path);
				myIntent.putExtra("CONFIGFILE", getDefaultConfigPath());
				myIntent.putExtra("IME", current_ime);
				startActivity(myIntent);
			}
			break;
		}
	}

	@Override
	public boolean onCreateOptionsMenu(Menu aMenu) {
		super.onCreateOptionsMenu(aMenu);
		getMenuInflater().inflate(R.menu.directory_list, aMenu);
		return true;
	}

	public void showPopup(View v) {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			PopupMenuAbstract menu = new PopupMenuAbstract(this, v);
			MenuInflater inflater = menu.getMenuInflater();
			inflater.inflate(R.menu.context_menu, menu.getMenu());
			menu.setOnMenuItemClickListener(new PopupMenuAbstract.OnMenuItemClickListener()
			{
				@Override
				public boolean onMenuItemClick(MenuItem item) {
					return onContextItemSelected(item);
				}
				
			});
			menu.show();
		}
		else
		{
			this.openContextMenu(findViewById(android.R.id.content));
		}
	}
	
	@Override
	public void onCreateContextMenu(ContextMenu menu, View v,
	                                ContextMenuInfo menuInfo) {
	    super.onCreateContextMenu(menu, v, menuInfo);
	    MenuInflater inflater = getMenuInflater();
	    inflater.inflate(R.menu.context_menu, menu);
	}
	
	@Override
	public boolean onOptionsItemSelected(MenuItem aItem) {
		switch (aItem.getItemId()) {
		case R.id.settings:
			showPopup(findViewById(R.id.settings));
			return true;

		default:
			return super.onOptionsItemSelected(aItem);
		}
	}
	
	@Override
	public boolean onContextItemSelected(MenuItem item) {
		switch (item.getItemId()) {
		case R.id.input_method_select:
			InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
			imm.showInputMethodPicker();
			return true;
			
		case R.id.rarch_settings:		
			Intent rset = new Intent(this, SettingsActivity.class);
			startActivity(rset);
			return true;
		case R.id.help:		
			Intent help = new Intent(this, HelpActivity.class);
			startActivity(help);
			return true;
			
		case R.id.report_ime:
			String current_ime = Settings.Secure.getString(getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
			new AlertDialog.Builder(this).setMessage(current_ime).setNeutralButton("Close", null).show();
			return true;
		case R.id.report_refreshrate:
			String current_rate = "Screen Refresh Rate: " + Double.valueOf(report_refreshrate).toString();
			new AlertDialog.Builder(this).setMessage(current_rate).setNeutralButton("Close", null).show();
			return true;

		case R.id.retroarch_guide:
			Intent rguide = new Intent(Intent.ACTION_VIEW, Uri.parse("http://www.libretro.com/documents/retroarch-manual.pdf"));
			startActivity(rguide);
			return true;

		case R.id.cores_guide:
			Intent cguide = new Intent(Intent.ACTION_VIEW, Uri.parse("http://www.libretro.com/documents/retroarch-cores-manual.pdf"));
			startActivity(cguide);
			return true;
			
		case R.id.overlay_guide:
			Intent mguide = new Intent(Intent.ACTION_VIEW, Uri.parse("http://www.libretro.com/documents/overlay.pdf"));
			startActivity(mguide);
			return true;
		case R.id.optimal_settings_device:
			detectDevice(true);
			return true;
		default:
			return false;
		}
	}
}

abstract class LazyPopupMenu {
	public abstract Menu getMenu();
	public abstract MenuInflater getMenuInflater();
	public abstract void setOnMenuItemClickListener(LazyPopupMenu.OnMenuItemClickListener listener);
	public abstract void show();
	public interface OnMenuItemClickListener {
		public abstract boolean onMenuItemClick(MenuItem item);
	}
}

@TargetApi(Build.VERSION_CODES.HONEYCOMB)
class HoneycombPopupMenu extends LazyPopupMenu {
	private PopupMenu instance;
	HoneycombPopupMenu.OnMenuItemClickListener listen;
	
	public HoneycombPopupMenu(Context context, View anchor)
	{
		instance = new PopupMenu(context, anchor);
	}

	@Override
	public void setOnMenuItemClickListener(HoneycombPopupMenu.OnMenuItemClickListener listener)
	{
		listen = listener;
		instance.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
			@Override
			public boolean onMenuItemClick(MenuItem item) {
				return listen.onMenuItemClick(item);
			}
			
		});
	}

	@Override
	public Menu getMenu() {
		return instance.getMenu();
	}

	@Override
	public MenuInflater getMenuInflater() {
		return instance.getMenuInflater();
	}

	@Override
	public void show() {
		instance.show();
	}
}

class PopupMenuAbstract extends LazyPopupMenu
{
	private LazyPopupMenu lazy;
	
	public PopupMenuAbstract(Context context, View anchor)
	{
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			lazy = new HoneycombPopupMenu(context, anchor);
		}
	}

	@Override
	public Menu getMenu() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			return lazy.getMenu();
		}
		else
		{
			return null;
		}
	}

	@Override
	public MenuInflater getMenuInflater() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			return lazy.getMenuInflater();
		}
		else
		{
			return null;
		}
	}

	@Override
	public void setOnMenuItemClickListener(PopupMenuAbstract.OnMenuItemClickListener listener) {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			lazy.setOnMenuItemClickListener(listener);
		}
	}

	@Override
	public void show() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			lazy.show();
		}
	}
}
