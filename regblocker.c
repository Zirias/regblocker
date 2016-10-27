#include <windows.h>
#include <stdio.h>
#include <string.h>

#define ROOTKEY HKEY_CURRENT_USER
#define BLOCKED_KEY_PATH "Software\\Policies\\Google"
#define DELETE_WAIT_MS 100

#define BUFSIZE 1024

char subKeyName[BUFSIZE];

static int deleteSubkeyRecursive(size_t subKeyNameLen, unsigned long availableSize);
static int findAndDeleteSubkeys(HKEY key, size_t subKeyNameLen, unsigned long availableSize);
static int checkAndDelete(void);

static int deleteSubkeyRecursive(size_t subKeyNameLen, unsigned long availableSize)
{
	fprintf(stderr, "delete subkey: %s\n", subKeyName);
	
	if (RegDeleteKey(ROOTKEY, subKeyName) == ERROR_SUCCESS) return 1;
	
	HKEY subkey;	
	if (RegOpenKeyEx(ROOTKEY, subKeyName, 0, KEY_ENUMERATE_SUB_KEYS, &subkey) != ERROR_SUCCESS) return 0;
	
	int rc = findAndDeleteSubkeys(subkey, subKeyNameLen, availableSize);
	RegCloseKey(subkey);
	if (!rc) return 0;
	
	return ((RegDeleteKey(ROOTKEY, subKeyName) == ERROR_SUCCESS));
}

static int findAndDeleteSubkeys(HKEY key, size_t subKeyNameLen, unsigned long availableSize)
{
	if (availableSize < 2UL) return 0;
	
	char *nameEnd = subKeyName + subKeyNameLen;
	if (*(nameEnd - 1) != '\\')
	{
		*nameEnd = '\\';
		*++nameEnd = 0;
		--availableSize;
		++subKeyNameLen;
	}
	
	unsigned long size = availableSize;
	while (RegEnumKeyEx(key, 0, nameEnd, &size, 0, 0, 0, 0) == ERROR_SUCCESS)
	{
		if (!deleteSubkeyRecursive(subKeyNameLen + size, availableSize - size))
		{
			*--nameEnd = 0;
			return 0;
		}
		size = availableSize;
	}
	
	*--nameEnd = 0;
	return 1;
}

static int checkAndDelete(void)
{
	HKEY blockedKey;	
	if (RegOpenKeyEx(ROOTKEY, BLOCKED_KEY_PATH, 0, KEY_ENUMERATE_SUB_KEYS, &blockedKey) != ERROR_SUCCESS)
	{
		perror("RegOpenKeyEx");
		return 0;
	}
		
	strcpy(subKeyName, BLOCKED_KEY_PATH);
	size_t subKeyNameLen = strlen(subKeyName);
	
	unsigned long availableSize = BUFSIZE - subKeyNameLen - 1;
	
	int rc = findAndDeleteSubkeys(blockedKey, subKeyNameLen, availableSize);	
	RegCloseKey(blockedKey);
	return rc;
}

int main(int argc, char **argv)
{
	HANDLE event = CreateEvent(0, 1, 0, 0);
	if (!event)
	{
		perror("CreateEvent");
		return -1;
	}
	
	if (!checkAndDelete()) return -1;
	
	for (;;) // ever
	{
		HKEY blockedKey;
		if (RegOpenKeyEx(ROOTKEY, BLOCKED_KEY_PATH, 0, KEY_NOTIFY, &blockedKey) != ERROR_SUCCESS)
		{
			perror("RegOpenKeyEx");
			return -1;
		}
	
		if (RegNotifyChangeKeyValue(blockedKey, 1, REG_NOTIFY_CHANGE_NAME, event, 1) != ERROR_SUCCESS)
		{
			perror("RegNotifyChangeKeyValue");
			RegCloseKey(blockedKey);
			return -1;
		}
		
		fprintf(stderr, "Waiting on %s ... ", BLOCKED_KEY_PATH);
    	if (WaitForSingleObject(event, INFINITE) == WAIT_FAILED)
	    {
		    perror("WaitForSingleObject");
			RegCloseKey(blockedKey);
		    return -1;
		}
		
		RegCloseKey(blockedKey);
		fputs("Key changed!\n", stderr);

		Sleep(DELETE_WAIT_MS);
		if (!checkAndDelete()) return -1;
	}
	
	return 0;
}