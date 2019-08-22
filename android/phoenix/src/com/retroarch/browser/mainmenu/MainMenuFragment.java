package com.retroarch.browser.mainmenu;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.util.Log;
import android.widget.Toast;

import com.retroarch.R;
import com.retroarch.browser.CoreSelection;
import com.retroarch.browser.HistorySelection;
import com.retroarch.browser.NativeInterface;
import com.retroarch.browser.dirfragment.DetectCoreDirectoryFragment;
import com.retroarch.browser.dirfragment.DirectoryFragment;
import com.retroarch.browser.dirfragment.DirectoryFragment.OnDirectoryFragmentClosedListener;
import com.retroarch.browser.mainmenu.gplwaiver.GPLWaiverDialogFragment;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.browser.retroactivity.RetroActivityFuture;
import com.retroarch.browser.retroactivity.RetroActivityPast;

/**
 * Represents the fragment that handles the layout of the main menu.
 */
public final class MainMenuFragment extends PreferenceListFragment implements OnPreferenceClickListener, OnDirectoryFragmentClosedListener
{
	private static final String TAG = "MainMenuFragment";
	private Context ctx;
	
	public Intent getRetroActivity()
	{
		if ((Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB))
		{
			return new Intent(ctx, RetroActivityFuture.class);
		}
		return new Intent(ctx, RetroActivityPast.class);
	}

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Cache the context
		this.ctx = getActivity();

		// Add the layout through the XML.
		addPreferencesFromResource(R.xml.main_menu);

		// Set the listeners for the menu items
		findPreference("resumeContentPref").setOnPreferenceClickListener(this);
		findPreference("loadCorePref").setOnPreferenceClickListener(this);
		findPreference("loadContentAutoPref").setOnPreferenceClickListener(this);
		findPreference("loadContentPref").setOnPreferenceClickListener(this);
		findPreference("loadContentHistoryPref").setOnPreferenceClickListener(this);
		findPreference("quitRetroArch").setOnPreferenceClickListener(this);

		// Extract assets. 
		extractAssets();

		final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(ctx);
		if (!prefs.getBoolean("first_time_refreshrate_calculate", false))
		{
			prefs.edit().putBoolean("first_time_refreshrate_calculate", true).commit();

			if (!detectDevice(false))
			{
				AlertDialog.Builder alert = new AlertDialog.Builder(ctx)
						.setTitle(R.string.welcome_to_retroarch)
						.setMessage(R.string.welcome_to_retroarch_desc)
						.setPositiveButton(R.string.ok, null);
				alert.show();
			}

			// First-run, so we show the GPL waiver agreement dialog.
			GPLWaiverDialogFragment.newInstance().show(getFragmentManager(), "gplWaiver");
		}
	}

	private void extractAssets()
	{
		if (areAssetsExtracted())
			return;

		final Dialog dialog = new Dialog(ctx);
		final Handler handler = new Handler();
		dialog.setContentView(R.layout.assets);
		dialog.setCancelable(false);
		dialog.setTitle(R.string.asset_extraction);

		// Java is fun :)
		Thread assetsThread = new Thread(new Runnable()
		{
			public void run()
			{
				extractAssetsThread();
				handler.post(new Runnable()
				{
					public void run()
					{
						dialog.dismiss();
					}
				});
			}
		});
		assetsThread.start();

		dialog.show();
	}

	// Extract assets from native code. Doing it from Java side is apparently unbearably slow ...
	private void extractAssetsThread()
	{
		try
		{
			final String dataDir = ctx.getApplicationInfo().dataDir;
			final String apk = ctx.getApplicationInfo().sourceDir;

			Log.i(TAG, "Extracting RetroArch assets from: " + apk + " ...");
			boolean success = NativeInterface.extractArchiveTo(apk, "assets", dataDir);
			if (!success) {
				throw new IOException("Failed to extract assets ...");
			}
			Log.i(TAG, "Extracted assets ...");

			File cacheVersion = new File(dataDir, ".cacheversion");
			DataOutputStream outputCacheVersion = new DataOutputStream(new FileOutputStream(cacheVersion, false));
			outputCacheVersion.writeInt(getVersionCode());
			outputCacheVersion.close();
		}
		catch (IOException e)
		{
			Log.e(TAG, "Failed to extract assets to cache.");
		}
	}

	private boolean areAssetsExtracted()
	{
		int version = getVersionCode();

		try
		{
			String dataDir = ctx.getApplicationInfo().dataDir;
			File cacheVersion = new File(dataDir, ".cacheversion");
			if (cacheVersion.isFile() && cacheVersion.canRead() && cacheVersion.canWrite())
			{
				DataInputStream cacheStream = new DataInputStream(new FileInputStream(cacheVersion));
				int currentCacheVersion = 0;
				try
				{
					currentCacheVersion = cacheStream.readInt();
					cacheStream.close();
				}
				catch (IOException ignored)
				{
				}

				if (currentCacheVersion == version)
				{
					Log.i("ASSETS", "Assets already extracted, skipping...");
					return true;
				}
			}
		}
		catch (IOException e)
		{
			Log.e(TAG, "Failed to extract assets to cache.");
			return false;
		}

		return false;
	}

	private int getVersionCode()
	{
		int version = 0;
		try
		{
			version = ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), 0).versionCode;
		}
		catch (NameNotFoundException ignored)
		{
		}

		return version;
	}

	private boolean detectDevice(boolean show_dialog)
	{
		boolean retval = false;

		final boolean mentionPlayStore = !Build.MODEL.equals("OUYA Console");
		final int messageId = (mentionPlayStore ? R.string.detect_device_msg_general : R.string.detect_device_msg_ouya);

		Log.i("Device MODEL", Build.MODEL);
		if (Build.MODEL.equals("SHIELD"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(ctx);
			alert.setTitle(R.string.nvidia_shield_detected);
			alert.setMessage(messageId);
			alert.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
			{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
					SharedPreferences prefs = UserPreferences.getPreferences(ctx);
					SharedPreferences.Editor edit = prefs.edit();
					edit.putString("video_refresh_rate", Double.toString(60.00d));
					edit.putBoolean("input_overlay_enable", false);
					edit.putBoolean("input_autodetect_enable", true);
					edit.putString("audio_latency", "64");
					edit.putBoolean("audio_latency_auto", true);
					edit.commit();
					UserPreferences.updateConfigFile(ctx);
				}
			});
			alert.show();
			retval = true;
		}
		else if (Build.MODEL.equals("GAMEMID_BT"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(ctx);
			alert.setTitle(R.string.game_mid_detected);
			alert.setMessage(messageId);
			alert.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
			{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
					SharedPreferences prefs = UserPreferences.getPreferences(ctx);
					SharedPreferences.Editor edit = prefs.edit();
					edit.putBoolean("input_overlay_enable", false);
					edit.putBoolean("input_autodetect_enable", true);
					edit.putString("audio_latency", "160");
					edit.putBoolean("audio_latency_auto", false);
					edit.commit();
					UserPreferences.updateConfigFile(ctx);
				}
			});
			alert.show();
			retval = true;
		}
		else if (Build.MODEL.equals("OUYA Console"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(ctx);
			alert.setTitle(R.string.ouya_detected);
			alert.setMessage(messageId);
			alert.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
			{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
					SharedPreferences prefs = UserPreferences.getPreferences(ctx);
					SharedPreferences.Editor edit = prefs.edit();
					edit.putBoolean("input_overlay_enable", false);
					edit.putBoolean("input_autodetect_enable", true);
					edit.putString("audio_latency", "64");
					edit.putBoolean("audio_latency_auto", true);
					edit.commit();
					UserPreferences.updateConfigFile(ctx);
				}
			});
			alert.show();
			retval = true;
		}
		else if (Build.MODEL.equals("R800x"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(ctx);
			alert.setTitle(R.string.xperia_play_detected);
			alert.setMessage(messageId);
			alert.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
			{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
					SharedPreferences prefs = UserPreferences.getPreferences(ctx);
					SharedPreferences.Editor edit = prefs.edit();
					edit.putBoolean("video_threaded", false);
					edit.putBoolean("input_overlay_enable", false);
					edit.putBoolean("input_autodetect_enable", true);
					edit.putString("video_refresh_rate", Double.toString(59.19132938771038));
					edit.putString("audio_latency", "128");
					edit.putBoolean("audio_latency_auto", false);
					edit.commit();
					UserPreferences.updateConfigFile(ctx);
				}
			});
			alert.show();
			retval = true;
		}
		else if (Build.ID.equals("JSS15J"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(ctx);
			alert.setTitle(R.string.nexus_7_2013_detected);
			alert.setMessage(messageId);
			alert.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
			{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
					SharedPreferences prefs = UserPreferences.getPreferences(ctx);
					SharedPreferences.Editor edit = prefs.edit();
					edit.putString("video_refresh_rate", Double.toString(59.65));
					edit.putString("audio_latency", "64");
					edit.putBoolean("audio_latency_auto", false);
					edit.commit();
					UserPreferences.updateConfigFile(ctx);
				}
			});
			alert.show();
			retval = true;
		}

		if (show_dialog)
		{
			Toast.makeText(ctx, R.string.no_optimal_settings, Toast.LENGTH_SHORT).show();
		}

		return retval;
	}

	@Override
	public boolean onPreferenceClick(Preference preference)
	{
		final String prefKey = preference.getKey();

		// Resume Content
		if (prefKey.equals("resumeContentPref"))
		{
			UserPreferences.updateConfigFile(ctx);

			final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(ctx);
			final String libretro_path = prefs.getString("libretro_path", ctx.getApplicationInfo().dataDir + "/cores");
			final String current_ime = Settings.Secure.getString(ctx.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
			final Intent retro = getRetroActivity();
			retro.putExtra("LIBRETRO", libretro_path);
			retro.putExtra("CONFIGFILE", UserPreferences.getDefaultConfigPath(ctx));
			retro.putExtra("IME", current_ime);
			startActivity(retro);
		}
		// Load Core Preference
		else if (prefKey.equals("loadCorePref"))
		{
			CoreSelection.newInstance().show(getFragmentManager(), "core_selection");
		}
		// Load ROM Preference
		else if (prefKey.equals("loadContentPref"))
		{
			final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(ctx);
			final String libretro_path = prefs.getString("libretro_path", ctx.getApplicationInfo().dataDir + "/cores");

			if (!new File(libretro_path).isDirectory())
			{
				final DirectoryFragment contentBrowser = DirectoryFragment.newInstance(R.string.load_content);
				contentBrowser.addDisallowedExts(".state", ".srm", ".state.auto", ".rtc");
				contentBrowser.setOnDirectoryFragmentClosedListener(this);
	
				final String startPath = prefs.getString("rgui_browser_directory", "");
				if (!startPath.isEmpty() && new File(startPath).exists())
					contentBrowser.setStartDirectory(startPath);
	
				contentBrowser.show(getFragmentManager(), "contentBrowser");
			}
			else
			{
				Toast.makeText(ctx, R.string.load_a_core_first, Toast.LENGTH_SHORT).show();
			}
		}
		else if (prefKey.equals("loadContentAutoPref"))
		{
			final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(ctx);
			final DetectCoreDirectoryFragment contentBrowser = DetectCoreDirectoryFragment.newInstance(R.string.load_content_auto);
			contentBrowser.addDisallowedExts(".state", ".srm", ".state.auto", ".rtc");
			contentBrowser.setOnDirectoryFragmentClosedListener(this);

			final String startPath = prefs.getString("rgui_browser_directory", "");
			if (!startPath.isEmpty() && new File(startPath).exists())
				contentBrowser.setStartDirectory(startPath);

			contentBrowser.show(getFragmentManager(), "contentBrowser");
		}
		// Load Content (History) Preference
		else if (prefKey.equals("loadContentHistoryPref"))
		{
			HistorySelection.newInstance().show(getFragmentManager(), "history_selection");
		}
		// Quit RetroArch preference
		else if (prefKey.equals("quitRetroArch"))
		{
			// TODO - needs to close entire app gracefully - including
			// NativeActivity if possible
			getActivity().finish();
		}

		return true;
	}

	@Override
	public void onDirectoryFragmentClosed(String path)
	{
		final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(ctx);
		final String libretro_path = prefs.getString("libretro_path", "");

		UserPreferences.updateConfigFile(ctx);
		String current_ime = Settings.Secure.getString(ctx.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
		Toast.makeText(ctx, String.format(getString(R.string.loading_data), path), Toast.LENGTH_SHORT).show();
		Intent retro = getRetroActivity();
		retro.putExtra("ROM", path);
		retro.putExtra("LIBRETRO", libretro_path);
		retro.putExtra("CONFIGFILE", UserPreferences.getDefaultConfigPath(ctx));
		retro.putExtra("IME", current_ime);
		startActivity(retro);
	}
}
