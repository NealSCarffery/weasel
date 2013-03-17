#include "stdafx.h"
#include "WeaselDeployer.h"
#include "Configurator.h"
#include "SwitcherSettingsDialog.h"
#include "UIStyleSettings.h"
#include "UIStyleSettingsDialog.h"
#include "DictManagementDialog.h"
#include <WeaselCommon.h>
#include <WeaselIPC.h>
#include <WeaselUtility.h>
#include <WeaselVersion.h>
#pragma warning(disable: 4005)
#pragma warning(disable: 4995)
#pragma warning(disable: 4996)
#include <rime_api.h>
#include <rime/deployer.h>
#include <rime/service.h>
#include <rime/lever/deployment_tasks.h>
#include <rime/lever/switcher_settings.h>
#pragma warning(default: 4996)
#pragma warning(default: 4995)
#pragma warning(default: 4005)


Configurator::Configurator()
{
}

void Configurator::Initialize()
{
	RimeTraits weasel_traits;
	weasel_traits.shared_data_dir = weasel_shared_data_dir();
	weasel_traits.user_data_dir = weasel_user_data_dir();
	const int len = 20;
	char utf8_str[len];
	memset(utf8_str, 0, sizeof(utf8_str));
	WideCharToMultiByte(CP_UTF8, 0, WEASEL_IME_NAME, -1, utf8_str, len - 1, NULL, NULL);
	weasel_traits.distribution_name = utf8_str;
	weasel_traits.distribution_code_name = WEASEL_CODE_NAME;
	weasel_traits.distribution_version = WEASEL_VERSION;
	RimeDeployerInitialize(&weasel_traits);
}

int Configurator::Run(bool installing)
{
    rime::Deployer& deployer(rime::Service::instance().deployer());
	bool reconfigured = false;

	rime::SwitcherSettings switcher_settings(&deployer);
	UIStyleSettings ui_style_settings(&deployer);

	bool skip_switcher_settings = installing && !switcher_settings.IsFirstRun();
	bool skip_ui_style_settings = installing && !ui_style_settings.IsFirstRun();

	(skip_switcher_settings || ConfigureSwitcher(&switcher_settings, &reconfigured)) &&
		(skip_ui_style_settings || ConfigureUI(&ui_style_settings, &reconfigured));

	if (installing || reconfigured) {
		return UpdateWorkspace(reconfigured);
	}
	return 0;
}

bool Configurator::ConfigureSwitcher(rime::SwitcherSettings* settings, bool* reconfigured)
{
    if (!settings->Load())
        return false;
	SwitcherSettingsDialog dialog(settings);
	if (dialog.DoModal() == IDOK) {
		if (settings->Save())
			*reconfigured = true;
		return true;
	}
	return false;
}

bool Configurator::ConfigureUI(UIStyleSettings* settings, bool* reconfigured) {
    if (!settings->Load())
        return false;
	UIStyleSettingsDialog dialog(settings);
	if (dialog.DoModal() == IDOK) {
		if (settings->Save())
			*reconfigured = true;
		return true;
	}
	return false;
}

int Configurator::UpdateWorkspace(bool report_errors) {
	HANDLE hMutex = CreateMutex(NULL, TRUE, L"WeaselDeployerMutex");
	if (!hMutex)
	{
		LOG(ERROR) << "Error creating WeaselDeployerMutex.";
		return 1;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		LOG(WARNING) << "another deployer process is running; aborting operation.";
		CloseHandle(hMutex);
		if (report_errors)
		{
			MessageBox(NULL, L"���ڈ�����һ헲����΄գ����u�������޸Č���ݔ�뷨�ٴΆ�������Ч��", L"��С�Ǻ���", MB_OK | MB_ICONINFORMATION);
		}
		return 1;
	}

	weasel::Client client;
	if (client.Connect())
	{
		LOG(INFO) << "Turning WeaselServer into maintenance mode.";
		client.StartMaintenance();
	}

	{
		// initialize default config, preset schemas
		RimeDeployWorkspace();
		// initialize weasel config
		RimeDeployConfigFile("weasel.yaml", "config_version");
	}

	CloseHandle(hMutex);  // should be closed before resuming service.

	if (client.Connect())
	{
		LOG(INFO) << "Resuming service.";
		client.EndMaintenance();
	}
	return 0;
}

int Configurator::DictManagement() {
	HANDLE hMutex = CreateMutex(NULL, TRUE, L"WeaselDeployerMutex");
	if (!hMutex)
	{
		LOG(ERROR) << "Error creating WeaselDeployerMutex.";
		return 1;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		LOG(WARNING) << "another deployer process is running; aborting operation.";
		CloseHandle(hMutex);
		MessageBox(NULL, L"���ڈ�����һ헲����΄գ�Ո�Ժ���ԇ��", L"��С�Ǻ���", MB_OK | MB_ICONINFORMATION);
		return 1;
	}

	weasel::Client client;
	if (client.Connect())
	{
		LOG(INFO) << "Turning WeaselServer into maintenance mode.";
		client.StartMaintenance();
	}

	{
		rime::Deployer& deployer(rime::Service::instance().deployer());
		rime::InstallationUpdate installation;
		installation.Run(&deployer);  // setup user data sync dir
		DictManagementDialog dlg(&deployer);
		dlg.DoModal();
	}

	CloseHandle(hMutex);  // should be closed before resuming service.

	if (client.Connect())
	{
		LOG(INFO) << "Resuming service.";
		client.EndMaintenance();
	}
	return 0;
}

int Configurator::SyncUserData() {
	HANDLE hMutex = CreateMutex(NULL, TRUE, L"WeaselDeployerMutex");
	if (!hMutex)
	{
		LOG(ERROR) << "Error creating WeaselDeployerMutex.";
		return 1;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		LOG(WARNING) << "another deployer process is running; aborting operation.";
		CloseHandle(hMutex);
		MessageBox(NULL, L"���ڈ�����һ헲����΄գ�Ո�Ժ���ԇ��", L"��С�Ǻ���", MB_OK | MB_ICONINFORMATION);
		return 1;
	}

	weasel::Client client;
	if (client.Connect())
	{
		LOG(INFO) << "Turning WeaselServer into maintenance mode.";
		client.StartMaintenance();
	}

	{
		rime::Deployer& deployer(rime::Service::instance().deployer());
		if (!RimeSyncUserData())
		{
			LOG(ERROR) << "Error synching user data.";
			return 1;
		}
		RimeJoinMaintenanceThread();
	}

	CloseHandle(hMutex);  // should be closed before resuming service.

	if (client.Connect())
	{
		LOG(INFO) << "Resuming service.";
		client.EndMaintenance();
	}
	return 0;
}
