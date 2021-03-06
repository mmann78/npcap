/*++

Copyright (c) Nmap.org.  All rights reserved.

Module Name:

	LoopbackRecord.cpp

Abstract:

	This is used for enumerating our "Npcap Loopback Adapter" using NetCfg API, if found, we changed its name from "Ethernet X" or "Local Network Area" to "Npcap Loopback Adapter".
	Also, we need to make a flag in registry to let the Npcap driver know that "this adapter is ours", so send the loopback traffic to it.

--*/

#pragma warning(disable: 4311 4312)

#include <Netcfgx.h>

#include <iostream>
#include <atlbase.h> // CComPtr
#include <devguid.h> // GUID_DEVCLASS_NET, ...

#include "LoopbackRecord.h"
#include "LoopbackRename.h"
#include "RegUtil.h"

#define			NPCAP_LOOPBACK_ADAPTER_NAME				NPF_DRIVER_NAME_NORMAL_WIDECHAR L" Loopback Adapter"
#define			NPCAP_LOOPBACK_APP_NAME					NPF_DRIVER_NAME_NORMAL_WIDECHAR L"_Loopback"

int g_NpcapAdapterID = -1;

// RAII helper class
class COM
{
public:
	COM();
	~COM();
};

COM::COM()
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

	if(!SUCCEEDED(hr))
	{
		_tprintf(_T("ERROR: CoInitializeEx() failed. Error code: 0x%08x\n"), hr);
	}
}

COM::~COM()
{
	CoUninitialize();
}

// RAII helper class
class NetCfg
{
	CComPtr<INetCfg> m_pINetCfg;
	CComPtr<INetCfgLock> m_pLock;

public:
	NetCfg();
	~NetCfg();

	// Displays all network adapters, clients, transport protocols and services.
	// For each client, transport protocol and service (network features) 
	//    shows adpater(s) they are bound to.
	BOOL GetNetworkConfiguration(); 
};

NetCfg::NetCfg() : m_pINetCfg(0)
{
	HRESULT hr = S_OK;

	hr = m_pINetCfg.CoCreateInstance(CLSID_CNetCfg);

	if(!SUCCEEDED(hr))
	{
		_tprintf(_T("ERROR: CoCreateInstance() failed. Error code: 0x%08x\n"), hr);
		throw 1;
	}

	hr = m_pINetCfg.QueryInterface(&m_pLock);

	if (!SUCCEEDED(hr))
	{
		_tprintf(_T("QueryInterface(INetCfgLock) 0x%08x\n"), hr);
		throw 2;
	}

	// Note that this call can block.
	hr = m_pLock->AcquireWriteLock(INFINITE, NPCAP_LOOPBACK_APP_NAME, NULL);
	if (!SUCCEEDED(hr))
	{
		_tprintf(_T("INetCfgLock::AcquireWriteLock 0x%08x\n"), hr);
		throw 3;
	}

	hr = m_pINetCfg->Initialize(NULL);

	if(!SUCCEEDED(hr))
	{
		_tprintf(_T("ERROR: Initialize() failed. Error code: 0x%08x\n"), hr);
		throw 4;
	}
}

NetCfg::~NetCfg()
{  
	HRESULT hr = S_OK;

	if(m_pINetCfg)
	{
		hr = m_pINetCfg->Uninitialize();
		if(!SUCCEEDED(hr))
		{
			_tprintf(_T("ERROR: Uninitialize() failed. Error code: 0x%08x\n"), hr);
		}

		hr = m_pLock->ReleaseWriteLock();
		if (!SUCCEEDED(hr))
		{
			_tprintf(_T("INetCfgLock::ReleaseWriteLock 0x%08x\n"), hr);
		}
	}
}

BOOL EnumerateComponents(CComPtr<INetCfg>& pINetCfg, const GUID* pguidClass)
{
	/*	cout << "\n\nEnumerating " << GUID2Str(pguidClass) << " class:\n" << endl;*/

	// IEnumNetCfgComponent provides methods that enumerate the INetCfgComponent interfaces 
	// for network components of a particular type installed on the operating system. 
	// Types of network components include network cards, protocols, services, and clients.
	CComPtr<IEnumNetCfgComponent> pIEnumNetCfgComponent;

	// get enumeration containing network components of the provided class (GUID)
	HRESULT hr = pINetCfg->EnumComponents(pguidClass, &pIEnumNetCfgComponent);

	if(!SUCCEEDED(hr))
	{
		_tprintf(_T("ERROR: Failed to get IEnumNetCfgComponent interface pointer\n"));
		throw 1;
	} 

	// INetCfgComponent interface provides methods that control and retrieve 
	// information about a network component.
	CComPtr<INetCfgComponent> pINetCfgComponent;

	unsigned int nIndex = 1;
	BOOL bFound = FALSE;
	BOOL bFailed = FALSE;
	// retrieve the next specified number of INetCfgComponent interfaces in the enumeration sequence.
	while(pIEnumNetCfgComponent->Next(1, &pINetCfgComponent, 0) == S_OK)
	{
		/*		cout << GUID2Desc(pguidClass) << " "<< nIndex++ << ":\n";*/

// 		LPWSTR pszDisplayName = NULL;
// 		pINetCfgComponent->GetDisplayName(&pszDisplayName);
// 		wcout << L"\tDisplay name: " << wstring(pszDisplayName) << L'\n';
// 		CoTaskMemFree(pszDisplayName);

		LPWSTR pszBindName = NULL;
		pINetCfgComponent->GetBindName(&pszBindName);
//		wcout << L"\tBind name: " << wstring(pszBindName) << L'\n';

// 		DWORD dwCharacteristics = 0;
// 		pINetCfgComponent->GetCharacteristics(&dwCharacteristics);
// 		cout << "\tCharacteristics: " << dwCharacteristics << '\n';
// 
// 		GUID guid;  
// 		pINetCfgComponent->GetClassGuid(&guid);
// 		cout << "\tClass GUID: " << guid.Data1 << '-' << guid.Data2 << '-'
// 			<< guid.Data3 << '-' << (unsigned int) guid.Data4 << '\n';
// 
// 		ULONG ulDeviceStatus = 0;
// 		pINetCfgComponent->GetDeviceStatus(&ulDeviceStatus);
// 		cout << "\tDevice Status: " << ulDeviceStatus << '\n';
// 
// 		LPWSTR pszHelpText = NULL;
// 		pINetCfgComponent->GetHelpText(&pszHelpText);
// 		wcout << L"\tHelp Text: " << wstring(pszHelpText) << L'\n';
// 		CoTaskMemFree(pszHelpText);
// 
// 		LPWSTR pszID = NULL;
// 		pINetCfgComponent->GetId(&pszID);
// 		wcout << L"\tID: " << wstring(pszID) << L'\n';
// 		CoTaskMemFree(pszID);
// 
// 		pINetCfgComponent->GetInstanceGuid(&guid);
// 		cout << "\tInstance GUID: " << guid.Data1 << '-' << guid.Data2 << '-'
// 			<< guid.Data3 << '-' << (unsigned int) guid.Data4 << '\n';

		LPWSTR pszPndDevNodeId = NULL;
		pINetCfgComponent->GetPnpDevNodeId(&pszPndDevNodeId);
//		wcout << L"\tPNP Device Node ID: " << wstring(pszPndDevNodeId) << L'\n';

		int iDevID = getIntDevID(pszPndDevNodeId);
		if (g_NpcapAdapterID == iDevID)
		{
			bFound = TRUE;

			hr = pINetCfgComponent->SetDisplayName(NPCAP_LOOPBACK_ADAPTER_NAME);

			if (hr != S_OK)
			{
				bFailed = TRUE;
			}

			if (!AddFlagToRegistry(pszBindName))
			{
				bFailed = TRUE;
			}

// 			if (!AddFlagToRegistry_Service(pszBindName))
// 			{
// 				bFailed = TRUE;
// 			}

			if (!RenameLoopbackNetwork(pszBindName))
			{
				bFailed = TRUE;
			}
		}

		CoTaskMemFree(pszBindName);
		CoTaskMemFree(pszPndDevNodeId);
		pINetCfgComponent.Release();

		if (bFound)
		{
			if (bFailed)
			{
				return FALSE;
			}
			else
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOL NetCfg::GetNetworkConfiguration()
{ 
	// get enumeration containing GUID_DEVCLASS_NET class of network components
	return EnumerateComponents(m_pINetCfg, &GUID_DEVCLASS_NET);
}

int getIntDevID(TCHAR strDevID[]) //DevID is in form like: "ROOT\\NET\\0008"
{
	int iDevID;
	_stscanf_s(strDevID, _T("ROOT\\NET\\%04d"), &iDevID);
	return iDevID;
}

BOOL AddFlagToRegistry(tstring strDeviceName)
{
	return WriteStrToRegistry(NPCAP_REG_KEY_NAME, NPCAP_REG_LOOPBACK_VALUE_NAME, tstring(_T("\\Device\\") + strDeviceName).c_str(), KEY_WRITE | KEY_WOW64_32KEY);
}

BOOL AddFlagToRegistry_Service(tstring strDeviceName)
{
	return WriteStrToRegistry(NPCAP_SERVICE_REG_KEY_NAME, NPCAP_REG_LOOPBACK_VALUE_NAME, tstring(_T("\\Device\\") + strDeviceName).c_str(), KEY_WRITE);
}

BOOL RecordLoopbackDevice(int iNpcapAdapterID)
{
	g_NpcapAdapterID = iNpcapAdapterID;

	try
	{
		COM com;
		NetCfg netCfg;
		if (!netCfg.GetNetworkConfiguration())
		{
			return FALSE;
		}
	}
	catch(...)
	{
		_tprintf(_T("ERROR: main() caught exception\n"));
		return FALSE;
	}

	return TRUE;
}