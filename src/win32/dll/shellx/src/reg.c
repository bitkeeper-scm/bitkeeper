/*
 * Copyright 2006,2008-2009,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"

static char *rootKeyNames[] = {
	"HKEY_LOCAL_MACHINE", "HKEY_USERS", "HKEY_CLASSES_ROOT",
	"HKEY_CURRENT_USER", "HKEY_CURRENT_CONFIG",
	"HKEY_PERFORMANCE_DATA", "HKEY_DYN_DATA", NULL
};

static HKEY rootKeys[] = {
	HKEY_LOCAL_MACHINE, HKEY_USERS, HKEY_CLASSES_ROOT, HKEY_CURRENT_USER,
	HKEY_CURRENT_CONFIG, HKEY_PERFORMANCE_DATA, HKEY_DYN_DATA
};

/*
 * The following table maps from registry types to strings.  Note that
 * the indices for this array are the same as the constants for the
 * known registry types so we don't need a separate table to hold the
 * mapping.
 */

#define	REG_INVALID_TYPE	10	// "invalid" below
static char *typeNames[] = {
    "none", "sz", "expand_sz", "binary", "dword",
    "dword_big_endian", "link", "multi_sz", "resource_list", NULL, "invalid"
};

static int	parseKeyName(char *key, HKEY *rootKey, char **keyName);
static int	recursiveDeleteKey(HKEY parent, char *key);

/*
 * Given a key and a value, return the data contained in that registry
 * entry. If len != 0, it contains the length of the data (useful if we
 * know the data is a string and don't care about the length.
 *
 * Returns NULL on failure. Caller must free the memory.
 */
void *
reg_get(char *key, char *value, long *len)
{
	HKEY	rootKey, hkey;
	char	*keyName;
	LPBYTE	data;
	unsigned long	tmp;
	int	err;

	if (parseKeyName(key, &rootKey, &keyName)) return (0);
	if (RegOpenKeyEx(rootKey, keyName, 0,
		KEY_QUERY_VALUE|KEY_WOW64_32KEY, &hkey)) return (0);
	/* We try with a 2K buffer, and keep retrying until we have
	 * the right size buffer
	 */
	data = (LPBYTE)malloc(2048);
	assert(data);
	tmp = 2048;
	while ((err = RegQueryValueEx(hkey, value, 0, 0, data, &tmp))
	    != ERROR_SUCCESS) {
		if (err == ERROR_MORE_DATA) {
			free(data);
			data = (LPBYTE)malloc(tmp);
			assert(data);
		} else {
			if (data) free(data);
			data = 0;
			if (len) *len = 0;
			goto out;
		}
	}
	if (len) *len = tmp;
 out:	RegCloseKey(hkey);
	return (data);
}

/*
 * If value isn't specified, creates the key if it doesn't already
 * exist. If value is specified, creates the key and value if
 * necessary. The contents of value are set to data with the type
 * indicated by type. If type isn't specified, the type sz is assumed.
 */
int
reg_set(char *key, char *value, DWORD type, void *data, long len)
{
	HKEY	hk, rootKey;
	char	*keyName;
	int	err;
	long	flags;

	unless(key) return (-1);
	if (value && !data) return (-1);
	if (parseKeyName(key, &rootKey, &keyName)) return (-1);
	/* RegCreateKey opens the key if it already exists and creates it
	   if it doesn't */
	if (err = RegCreateKeyEx(rootKey, keyName, 0, 0,
	    REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS|KEY_WOW64_32KEY, 0,
	    &hk, &flags))
		return (-1);
	unless(value) {
		/* we were asked to just create the key */
		err = 0;
		goto out;
	}
	unless(type) type = REG_SZ;
	switch (type) {
	    case REG_DWORD:
		err = RegSetValueEx(hk, value, 0, type,
		    (BYTE *)data, sizeof(DWORD));
		break;
	    case REG_SZ:
	    case REG_EXPAND_SZ:
		unless (len) len = data?(int)strlen((char *)data):0;
		/* fall through */
	    case REG_BINARY:
		err = RegSetValueEx(hk, value, 0, type,
		    (BYTE *)data, (DWORD) len);
		break;
	    default:
		err = -1;
		break;
	}
out:	RegCloseKey(hk);
	return (err);
}

int
reg_broadcast(char *key, int timeout)
{
	DWORD_PTR sendResult;

	unless (timeout) timeout = 3000;	/* default to 3s */
	return (!SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE,
	    (WPARAM) 0, (LPARAM) key, SMTO_ABORTIFHUNG, timeout, &sendResult));
}

/*
 * returns the type of the value in the key
 */
DWORD
reg_type(char *key, char *value)
{
	HKEY	hk, rootKey;
	char	*keyName;
	DWORD	type = REG_INVALID_TYPE; /* invalid type */

	if (parseKeyName(key, &rootKey, &keyName)) return (-1);
	if (RegOpenKeyEx(rootKey, keyName, 0,
	    KEY_QUERY_VALUE|KEY_WOW64_32KEY, &hk))
		return (-1);
	if (RegQueryValueEx(hk, value, 0, &type, 0, 0)) type = REG_INVALID_TYPE;
	RegCloseKey(hk);
	return (type);
}

/*
 * returns a string that corresponds to the type
 */
char *
reg_typestr(DWORD type)
{
	if (type < 0 || type > REG_INVALID_TYPE) type = REG_INVALID_TYPE;
	return (typeNames[type]);
}

/*
 * If value is not NULL, the specified value under the key will be
 * deleted from the registry. If the value is NULL, the specified key
 * and any subkeys or values beneath it in the registry heirarchy will
 * be deleted. If the key could not be deleted then it returns -1. If
 * the key did not exist, the command has no effect and it returns 0.
 */
int
reg_delete(char *key, char *value)
{
	HKEY	rootKey, hk;
	char	*keyName, *tail, *keycopy;
	int	err;

	if (parseKeyName(key, &rootKey, &keyName)) return (-1);
	if (value) {
		/* delete value */
		if (err = RegOpenKeyEx(rootKey, keyName, 0,
			KEY_SET_VALUE|KEY_WOW64_32KEY, &hk)) return (err);
		err = RegDeleteValue(hk, value);
		RegCloseKey(hk);
		return (err);
	} else {
		/* delete key */
		unless (*keyName) return (-1); /* cannot delete root keys */
		/* keyName might be a pointer to a constant string
		 * (e.g. #define) */
		keycopy = strdup(keyName);
		if (tail = strrchr(keycopy, '\\')) {
			*tail++ = 0;
		} else {
			tail = keycopy; keyName = 0;
		}
		if (err = RegOpenKeyEx(rootKey, keycopy, 0,
		    KEY_ENUMERATE_SUB_KEYS|DELETE|KEY_WOW64_32KEY, &hk)) {
			if (err == ERROR_FILE_NOT_FOUND) {
				/* doesn't exist */
				err = 0;
				goto out;
			}
			/* unable to delete key */
			err = -1;
			goto out;
		}
		err = recursiveDeleteKey(hk, tail);
		RegCloseKey(hk);
out:		free(keycopy);
		return (err);
	}
	/* NOT REACHED */
	assert("unreachable value reached" == 0);
}

/*
 * returns an addLine list of names of all the subkeys of key
 */
char **
reg_keys(char *key)
{
	HKEY	hk, rootKey;
	char	*keyName, **result = 0;
	long	idx;
	char	buf[MAX_PATH+1];

	if (parseKeyName(key, &rootKey, &keyName)) return (0);
	if (RegOpenKeyEx(rootKey, keyName, 0,
	    KEY_ENUMERATE_SUB_KEYS|KEY_WOW64_32KEY, &hk))
		return (0);
	for(idx = 0;
	    RegEnumKey(hk, idx, buf, MAX_PATH+1) == ERROR_SUCCESS;
	    idx++) {
		result = addLine(result, strdup(buf));
	}
	RegCloseKey(hk);
	return (result);
}

/*
 * returns an addLine list of names of all the values of key
 */
char **
reg_values(char *key)
{
	HKEY	hk, rootKey;
	char	*keyName, **result = 0;
	long	idx, size, maxSize;
	char	*buf;

	if (parseKeyName(key, &rootKey, &keyName)) return (0);
	if (RegOpenKeyEx(rootKey, keyName, 0,
		KEY_QUERY_VALUE|KEY_WOW64_32KEY, &hk)) return (0);
	if (RegQueryInfoKey(hk, 0, 0, 0, 0, 0, 0,
	       &idx, &maxSize, 0, 0, 0)) goto out;
	/* values could have wide chars, so reserve twice the space needed */
	buf = malloc(++maxSize * 2);
	assert(buf);
	for (idx = 0, size = maxSize;
	     RegEnumValue(hk, idx, buf, &size, 0, 0, 0, 0) == ERROR_SUCCESS;
	     idx++, size = maxSize) {
		result = addLine(result, strdup(buf));
	}
	free(buf);
out:	RegCloseKey(hk);
	return (result);
}


static int
recursiveDeleteKey(HKEY parent, char *key)
{
	int	err;
	long	maxSize, size;
	char	*subkey;
	HKEY	hk;

	if (!key || *key == 0) return (-1);
	if ((err = RegOpenKeyEx(parent, key, 0, KEY_ENUMERATE_SUB_KEYS |
	    DELETE | KEY_QUERY_VALUE | KEY_WOW64_32KEY, &hk))) return err;
	if ((err = RegQueryInfoKey(hk, 0, 0, 0, 0, &maxSize,
		 0, 0, 0, 0, 0, 0))) return err;
	/* subkeys could have wide chars, so reserve twice the space needed */
	subkey = malloc(++maxSize * 2);
	assert(subkey);
	err = ERROR_SUCCESS;
	while (err == ERROR_SUCCESS) {
		size = maxSize;
		err = RegEnumKeyEx(hk, 0, subkey, &size, 0, 0, 0, 0);
		if (err == ERROR_NO_MORE_ITEMS) {
			err = RegDeleteKey(parent, key);
			break;
		} else if (err == ERROR_SUCCESS) {
			err = recursiveDeleteKey(hk, subkey);
		}
	}
	free(subkey);
	RegCloseKey(hk);
	return (err);
}

static int
parseKeyName(char *key, HKEY *rootKey, char **keyName)
{
	char	*rootName = 0;
	int	i;

	if (key[0] == '\\' && key[1] == '\\') {
		/* remote keys (e.g. \\machine\HKEY_LOCAL_MACHINE\key
		 * are not supported
		 */
		return (-1);
	}
	if (key[0] == '\\') {
		/* invalid root (hive) */
		return (-2);
	}
	/* Split the key into root and subkey */
	for (*keyName = key; **keyName != 0; (*keyName)++) {
		if (**keyName == '\\') {
			/* reserve twice the space needed in case there are
			 * wide chars in the root
			 */
			rootName = malloc((*keyName-key)*2);
			assert(rootName);
			strncpy(rootName, key, (*keyName-key));
			rootName[*keyName-key] = 0;
			(*keyName)++;
			break;
		}
	}
	unless(rootName) rootName = key;
	for (i = 0; rootKeyNames[i] != 0; i++) {
		if (streq(rootName, rootKeyNames[i])) break;
	}
	if (rootKeyNames[i] == 0) {
		/* invalid key */
		free(rootName);
		return (-3);
	}
	*rootKey = rootKeys[i];
	if (rootName != key) free(rootName);
	return (0);
}
