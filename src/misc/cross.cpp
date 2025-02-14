/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "cross.h"

#include <cerrno>
#include <string>
#include <vector>

#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef WIN32
#ifndef _WIN32_IE
#define _WIN32_IE 0x0400
#endif
#include <shlobj.h>
#else
#include <libgen.h>
#endif

#if defined HAVE_PWD_H
#include <pwd.h>
#endif

#include "fs_utils.h"
#include "string_utils.h"
#include "support.h"

static std::string GetConfigName()
{
	return "dosbox-staging.conf";
}

#ifndef WIN32

std::string cached_conf_path;

#if defined(MACOSX)

static std::string DetermineConfigPath()
{
	const std::string conf_path = CROSS_ResolveHome("~/Library/Preferences/DOSBox");
	mkdir(conf_path.c_str(), 0700);
	return conf_path;
}

#else

static bool CreateDirectories(const std::string &path)
{
	struct stat sb;
	if (stat(path.c_str(), &sb) == 0) {
		const bool is_dir = ((sb.st_mode & S_IFMT) == S_IFDIR);
		return is_dir;
	}

	std::vector<char> tmp(path.begin(), path.end());
	std::string dname = dirname(tmp.data());

	// Create parent directories recursively
	if (!CreateDirectories(dname))
		return false;

	return (mkdir(path.c_str(), 0700) == 0);
}

static std::string DetermineConfigPath()
{
	const char *xdg_conf_home = getenv("XDG_CONFIG_HOME");
	const std::string conf_home = xdg_conf_home ? xdg_conf_home : "~/.config";
	const std::string conf_path = CROSS_ResolveHome(conf_home + "/dosbox");
	const std::string old_conf_path = CROSS_ResolveHome("~/.dosbox");

	if (path_exists(conf_path + "/" + GetConfigName())) {
		return conf_path;
	}

	if (path_exists(old_conf_path + "/" + GetConfigName())) {
		LOG_MSG("WARNING: Config file found in deprecated path! (~/.dosbox)\n"
		        "Backup/remove this dir and restart to generate updated config file.\n"
		        "---");
		return old_conf_path;
	}

	if (!CreateDirectories(conf_path)) {
		LOG_MSG("ERROR: Directory '%s' cannot be created",
		        conf_path.c_str());
		return old_conf_path;
	}

	return conf_path;
}

#endif // !MACOSX

void CROSS_DetermineConfigPaths()
{
	if (cached_conf_path.empty())
		cached_conf_path = DetermineConfigPath();
}

#endif // !WIN32

#ifdef WIN32

void CROSS_DetermineConfigPaths() {}

static void W32_ConfDir(std::string& in,bool create) {
	int c = create?1:0;
	char result[MAX_PATH] = { 0 };
	BOOL r = SHGetSpecialFolderPath(NULL,result,CSIDL_LOCAL_APPDATA,c);
	if(!r || result[0] == 0) r = SHGetSpecialFolderPath(NULL,result,CSIDL_APPDATA,c);
	if(!r || result[0] == 0) {
		char const * windir = getenv("windir");
		if(!windir) windir = "c:\\windows";
		safe_strcpy(result, windir);
		char const* appdata = "\\Application Data";
		size_t len = safe_strlen(result);
		if (len + strlen(appdata) < MAX_PATH)
			safe_strcat(result, appdata);
		if (create)
			mkdir(result);
	}
	in = result;
}
#endif

std::string CROSS_GetPlatformConfigDir()
{
	// Cache the result, as this doesn't change
	static std::string conf_dir = {};
	if (conf_dir.length())
		return conf_dir;

	// Check if a portable layout exists
	std::string config_file;
	Cross::GetPlatformConfigName(config_file);
	const auto portable_conf_path = GetExecutablePath() / config_file;
	if (std_fs::is_regular_file(portable_conf_path)) {
		conf_dir = portable_conf_path.parent_path().string();
		LOG_MSG("CONFIG: Using portable configuration layout in %s",
		        conf_dir.c_str());
		conf_dir += CROSS_FILESPLIT;
		return conf_dir;
	}

	// Otherwise get the OS-specific configuration directory
#ifdef WIN32
	W32_ConfDir(conf_dir, false);
	conf_dir += "\\DOSBox\\";
#else
	assert(!cached_conf_path.empty());
	conf_dir = cached_conf_path;
	if (conf_dir.back() != CROSS_FILESPLIT)
		conf_dir += CROSS_FILESPLIT;
#endif
	return conf_dir;
}

void Cross::GetPlatformConfigDir(std::string &in)
{
	in = CROSS_GetPlatformConfigDir();
}

void Cross::GetPlatformConfigName(std::string &in)
{
	in = GetConfigName();
}

void Cross::ResolveHomedir(std::string &in)
{
	in = CROSS_ResolveHome(in);
}

void Cross::CreatePlatformConfigDir(std::string &in)
{
#ifdef WIN32
	W32_ConfDir(in,true);
	in += "\\DOSBox";
#else
	assert(!cached_conf_path.empty());
	in = cached_conf_path.c_str();
#endif
	if (in.back() != CROSS_FILESPLIT)
		in += CROSS_FILESPLIT;

	if (create_dir(in.c_str(), 0700, OK_IF_EXISTS) != 0) {
		LOG_MSG("ERROR: Creation of config directory '%s' failed: %s",
		        in.c_str(), safe_strerror(errno).c_str());
	}
}

std::string CROSS_ResolveHome(const std::string &str)
{
	if (!str.size() || str[0] != '~') // No ~
		return str;

	std::string temp_line = str;
	if(temp_line.size() == 1 || temp_line[1] == CROSS_FILESPLIT) { //The ~ and ~/ variant
		char * home = getenv("HOME");
		if(home) temp_line.replace(0,1,std::string(home));
#if defined HAVE_SYS_TYPES_H && defined HAVE_PWD_H
	} else { // The ~username variant
		std::string::size_type namelen = temp_line.find(CROSS_FILESPLIT);
		if(namelen == std::string::npos) namelen = temp_line.size();
		std::string username = temp_line.substr(1,namelen - 1);
		struct passwd* pass = getpwnam(username.c_str());
		if(pass) temp_line.replace(0,namelen,pass->pw_dir); //namelen -1 +1(for the ~)
#endif // USERNAME lookup code
	}
	return temp_line;
}

bool Cross::IsPathAbsolute(std::string const& in) {
	// Absolute paths
#if defined (WIN32)
	// drive letter
	if (in.size() > 2 && in[1] == ':' ) return true;
	// UNC path
	else if (in.size() > 2 && in[0]=='\\' && in[1]=='\\') return true;
#else
	if (in.size() > 1 && in[0] == '/' ) return true;
#endif
	return false;
}

#if defined (WIN32)

dir_information* open_directory(const char* dirname) {
	if (dirname == NULL) return NULL;

	size_t len = strlen(dirname);
	if (len == 0) return NULL;

	static dir_information dir;

	safe_strncpy(dir.base_path,dirname,MAX_PATH);

	if (dirname[len - 1] == '\\')
		safe_strcat(dir.base_path, "*.*");
	else
		safe_strcat(dir.base_path, "\\*.*");

	dir.handle = INVALID_HANDLE_VALUE;

	return (path_exists(dirname) ? &dir : nullptr);
}

bool read_directory_first(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	dirp->handle = FindFirstFile(dirp->base_path, &dirp->search_data);
	if (INVALID_HANDLE_VALUE == dirp->handle) {
		return false;
	}

	safe_strncpy(entry_name,dirp->search_data.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);

	if (dirp->search_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

bool read_directory_next(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	int result = FindNextFile(dirp->handle, &dirp->search_data);
	if (result==0) return false;

	safe_strncpy(entry_name,dirp->search_data.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);

	if (dirp->search_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

void close_directory(dir_information* dirp) {
	if (dirp && dirp->handle != INVALID_HANDLE_VALUE) {
		FindClose(dirp->handle);
		dirp->handle = INVALID_HANDLE_VALUE;
	}
}

#else

dir_information* open_directory(const char* dirname) {
	static dir_information dir;
	dir.dir=opendir(dirname);
	safe_strcpy(dir.base_path, dirname);
	return dir.dir?&dir:NULL;
}

bool read_directory_first(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	return read_directory_next(dirp,entry_name,is_directory);
}

bool read_directory_next(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	struct dirent* dentry = readdir(dirp->dir);
	if (dentry==NULL) {
		return false;
	}

//	safe_strncpy(entry_name,dentry->d_name,(FILENAME_MAX<MAX_PATH)?FILENAME_MAX:MAX_PATH);	// [include stdio.h], maybe pathconf()
	safe_strncpy(entry_name,dentry->d_name,CROSS_LEN);

	// TODO check if this check can be replaced with glibc-defined
	// _DIRENT_HAVE_D_TYPE. Non-GNU systems (BSD) provide d_type field as
	// well, but do they provide define?
	// Alternatively, maybe we can replace whole directory listing with
	// C++17 std::filesystem::directory_iterator.
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
	if (dentry->d_type == DT_DIR) {
		is_directory = true;
		return true;
	} else if (dentry->d_type == DT_REG) {
		is_directory = false;
		return true;
	}
#endif

	// Maybe only for DT_UNKNOWN if HAVE_STRUCT_DIRENT_D_TYPE
	static char buffer[2 * CROSS_LEN + 1] = { 0 };
	static char split[2] = { CROSS_FILESPLIT , 0 };
	buffer[0] = 0;
	safe_strcpy(buffer, dirp->base_path);
	size_t buflen = safe_strlen(buffer);
	if (buflen && buffer[buflen - 1] != CROSS_FILESPLIT)
		safe_strcat(buffer, split);
	safe_strcat(buffer, entry_name);
	struct stat status;

	if (stat(buffer,&status) == 0) is_directory = (S_ISDIR(status.st_mode)>0);
	else is_directory = false;

	return true;
}

void close_directory(dir_information* dirp) {
	if (dirp) closedir(dirp->dir);
}

#endif

FILE *fopen_wrap(const char *path, const char *mode) {
#if defined(WIN32)
	;
#elif defined (MACOSX)
	;
#else  
#if defined (HAVE_REALPATH)
	char work[CROSS_LEN] = {0};
	strncpy(work,path,CROSS_LEN-1);
	char* last = strrchr(work,'/');
	
	if (last) {
		if (last != work) {
			*last = 0;
			//If this compare fails, then we are dealing with files in / 
			//Which is outside the scope, but test anyway. 
			//However as realpath only works for exising files. The testing is 
			//in that case not done against new files.
		}
		char* check = realpath(work,NULL);
		if (check) {
			if ( ( strlen(check) == 5 && strcmp(check,"/proc") == 0) || strncmp(check,"/proc/",6) == 0) {
//				LOG_MSG("lst hit %s blocking!",path);
				free(check);
				return NULL;
			}
			free(check);
		}
	}

#if 0
//Lightweight version, but then existing files can still be read, which is not ideal	
	if (strpbrk(mode,"aw+") != NULL) {
		LOG_MSG("pbrk ok");
		char* check = realpath(path,NULL);
		//Will be null if file doesn't exist.... ENOENT
		//TODO What about unlink /proc/self/mem and then create it ?
		//Should be safe for what we want..
		if (check) {
			if (strncmp(check,"/proc/",6) == 0) {
				free(check);
				return NULL;
			}
			free(check);
		}
	}
*/
#endif //0 

#endif //HAVE_REALPATH
#endif

	return fopen(path,mode);
}

// A helper for fopen_wrap that will fallback to read-only if read-write isn't possible.
// In the fallback case, is_readonly argument is toggled to true.
// In all cases, a pointer to the file is returned (or nullptr on failure).
FILE *fopen_wrap_ro_fallback(const std::string &filename, bool &is_readonly)
{
	// Try with the requested permissions
	const auto requested_perms = is_readonly ? "rb" : "rb+";
	FILE *fp = fopen_wrap(filename.c_str(), requested_perms);
	if (fp || is_readonly) {
		return fp;
	}
	// Fallback to read-only
	assert(!fp && !is_readonly);
	fp = fopen_wrap(filename.c_str(), "rb");
	if (fp) {
		is_readonly = true;
		LOG_INFO("FILESYSTEM: Opened %s read-only per host filesystem permissions",
		         filename.c_str());
	}
	// Note: if failed, the caller should provide a context-specific message
	return fp;
}

namespace cross {

#if defined(WIN32)
struct tm *localtime_r(const time_t *timep, struct tm *result)
{
	const errno_t err = localtime_s(result, timep);
	return (err == 0 ? result : nullptr);
}
#endif

} // namespace cross
