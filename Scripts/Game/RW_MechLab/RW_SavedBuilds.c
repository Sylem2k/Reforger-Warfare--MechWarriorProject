//------------------------------------------------------------------------------------------------
//! REFORGER WARFARE - SAVED BUILDS. Players save frame loadouts in the Mech Lab and order them
//! ready-made at a Mech Bay. Stored on the SERVER, one file per player identity (the same id
//! the backend gives the player on every server session):
//!     $profile:RW_MechBuilds/<identity id>.txt
//! One line per build:  frame prefab GUID | name | extra heatsinks | module ids (comma separated)
//! Every saved build is checked again with the frame rules when it is used.

//------------------------------------------------------------------------------------------------
class RW_SavedBuild
{
	string m_sFrame;                 // frame prefab GUID, e.g. {33D70479FF3498E0}
	string m_sName;
	int m_iHeatsinks;
	ref array<int> m_aModules = {};

	//------------------------------------------------------------------------------------------------
	string ToLine()
	{
		string modules = "";
		for (int i = 0; i < m_aModules.Count(); i++)
		{
			if (i > 0)
				modules = modules + ",";
			modules = modules + m_aModules[i].ToString();
		}
		return m_sFrame + "|" + m_sName + "|" + m_iHeatsinks.ToString() + "|" + modules;
	}

	//------------------------------------------------------------------------------------------------
	static RW_SavedBuild FromLine(string line)
	{
		array<string> parts = {};
		line.Split("|", parts, false);
		if (parts.Count() < 4)
			return null;
		RW_SavedBuild build = new RW_SavedBuild();
		build.m_sFrame = parts[0];
		build.m_sName = parts[1];
		build.m_iHeatsinks = parts[2].ToInt();
		array<string> ids = {};
		parts[3].Split(",", ids, true);
		foreach (string id : ids)
			build.m_aModules.Insert(Math.Clamp(id.ToInt(), 0, 31));
		return build;
	}

	//------------------------------------------------------------------------------------------------
	//! "{GUID}" part of a prefab name (paths can differ, the GUID can't).
	static string GuidOf(string resource)
	{
		int close = resource.IndexOf("}");
		if (resource.StartsWith("{") && close > 0)
			return resource.Substring(0, close + 1);
		return resource;
	}

	//------------------------------------------------------------------------------------------------
	//! Frame GUID of a mech (its prefab).
	static string FrameOf(IEntity mech)
	{
		if (!mech || !mech.GetPrefabData())
			return "";
		return GuidOf(mech.GetPrefabData().GetPrefabName());
	}

	//------------------------------------------------------------------------------------------------
	//! Text safe for the file format (no separators / new lines).
	static string Clean(string text)
	{
		text.Replace("|", "/");
		text.Replace("\n", " ");
		text.Replace("\r", " ");
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! All builds in a block of lines (what the server sends the client).
	static void ParseAll(string data, notnull array<ref RW_SavedBuild> outBuilds)
	{
		outBuilds.Clear();
		array<string> lines = {};
		data.Split("\n", lines, true);
		foreach (string line : lines)
		{
			RW_SavedBuild build = FromLine(line);
			if (build)
				outBuilds.Insert(build);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The 'slot'-th build (1-based) for this frame, in list order. Null = none.
	static RW_SavedBuild FindSlot(array<ref RW_SavedBuild> builds, string frame, int slot)
	{
		int n = 0;
		foreach (RW_SavedBuild build : builds)
		{
			if (build.m_sFrame != frame)
				continue;
			n++;
			if (n == slot)
				return build;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	static int CountFor(array<ref RW_SavedBuild> builds, string frame)
	{
		int n = 0;
		foreach (RW_SavedBuild build : builds)
		{
			if (build.m_sFrame == frame)
				n++;
		}
		return n;
	}
}

//------------------------------------------------------------------------------------------------
//! Server-side store: loads / saves each player's builds file (cached while the server runs).
class RW_BuildStore
{
	static const string FOLDER = "$profile:RW_MechBuilds";
	static const int MAX_PER_FRAME = 5;

	protected static ref map<string, ref array<ref RW_SavedBuild>> s_mCache;

	//------------------------------------------------------------------------------------------------
	//! Identity id of a player (same on every session). Offline / Workbench without one: the name.
	static string KeyFor(int playerId)
	{
		string key;
		if (GetGame().GetBackendApi())
			key = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
		if (key.IsEmpty())
			key = "local_" + GetGame().GetPlayerManager().GetPlayerName(playerId);

		// File-name safe.
		string safe = "";
		for (int i = 0; i < key.Length(); i++)
		{
			string c = key.Get(i);
			int code = c.ToAscii();
			if ((code >= 48 && code <= 57) || (code >= 65 && code <= 90) || (code >= 97 && code <= 122) || code == 45 || code == 95)
				safe = safe + c;
		}
		if (safe.IsEmpty())
			safe = "unknown";
		return safe;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref RW_SavedBuild> Get(string key)
	{
		if (!s_mCache)
			s_mCache = new map<string, ref array<ref RW_SavedBuild>>();
		array<ref RW_SavedBuild> builds = s_mCache.Get(key);
		if (builds)
			return builds;

		builds = {};
		string path = FOLDER + "/" + key + ".txt";
		if (FileIO.FileExists(path))
		{
			FileHandle file = FileIO.OpenFile(path, FileMode.READ);
			if (file)
			{
				string line;
				for (int guard = 0; guard < 500; guard++)
				{
					if (file.ReadLine(line) < 0 || line.IsEmpty())
						break;
					RW_SavedBuild build = RW_SavedBuild.FromLine(line);
					if (build)
						builds.Insert(build);
				}
				file.Close();
			}
		}
		s_mCache.Set(key, builds);
		return builds;
	}

	//------------------------------------------------------------------------------------------------
	static void Write(string key)
	{
		array<ref RW_SavedBuild> builds = Get(key);
		FileIO.MakeDirectory(FOLDER);
		FileHandle file = FileIO.OpenFile(FOLDER + "/" + key + ".txt", FileMode.WRITE);
		if (!file)
		{
			Print("[RW BUILDS] could not write " + FOLDER + "/" + key + ".txt", LogLevel.WARNING);
			return;
		}
		foreach (RW_SavedBuild build : builds)
			file.WriteLine(build.ToLine());
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! All of a player's builds as text (one line each) for the client.
	static string Pack(string key)
	{
		string data = "";
		foreach (RW_SavedBuild build : Get(key))
			data = data + build.ToLine() + "\n";
		return data;
	}

	//------------------------------------------------------------------------------------------------
	//! Add a build; a frame keeps MAX_PER_FRAME, the oldest goes. Returns the build's slot number.
	static int Add(string key, RW_SavedBuild build)
	{
		array<ref RW_SavedBuild> builds = Get(key);
		while (RW_SavedBuild.CountFor(builds, build.m_sFrame) >= MAX_PER_FRAME)
		{
			for (int i = 0; i < builds.Count(); i++)
			{
				if (builds[i].m_sFrame == build.m_sFrame)
				{
					builds.RemoveOrdered(i);
					break;
				}
			}
		}
		builds.Insert(build);
		Write(key);
		return RW_SavedBuild.CountFor(builds, build.m_sFrame);
	}

	//------------------------------------------------------------------------------------------------
	static bool Remove(string key, string frame, int slot)
	{
		array<ref RW_SavedBuild> builds = Get(key);
		RW_SavedBuild build = RW_SavedBuild.FindSlot(builds, frame, slot);
		if (!build)
			return false;
		builds.RemoveItemOrdered(build);
		Write(key);
		return true;
	}
}
