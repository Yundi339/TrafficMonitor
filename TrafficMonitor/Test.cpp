#include "stdafx.h"
#include "Test.h"
#include "Common.h"
#include "SkinFile.h"
#include "TrafficMonitor.h"
#include "IniHelper.h"
#include "PluginUpdateHelper.h"
#include "MessageDlg.h"
#include "HistoryTrafficFile.h"
#include "HistoryTrafficRetrySchedule.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void TestHttpQequest()
{
    wstring result;
    bool rtn = CCommon::GetURL(L"https://v4.yinghualuo.cn/bejson", result, true, L"TrafficMonitor_V1.78");
    int a = 0;
}

static void TestGetLicense()
{
    CString license_str;
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDR_LICENSE), _T("TEXT"));
    if (hRes != NULL)
    {
        HGLOBAL hglobal = LoadResource(NULL, hRes);
        if (hglobal != NULL)
        {
            license_str = CCommon::StrToUnicode((const char*)hglobal, true).c_str();
            int a = 0;
        }
    }
}

static void TestSkin()
{
    CSkinFile skin;
    skin.Load(L"0默认皮肤");
    int a = 0;
}

static void TestCrash()
{
    CString* pStr = nullptr;
    int a = pStr->GetLength();
    printf("%d", a);

}

static void TestPlugin()
{
    if (!theApp.m_plugins.GetPlugins().empty())
    {
        theApp.m_plugins.GetPlugins()[0].plugin->ShowOptionsDialog(theApp.m_pMainWnd->m_hWnd);
    }
}

static void TestDate()
{
    Date d;
    d.year = 2021;
    d.month = 1;
    d.day = 4;
    int week = d.week();
    int a = 0;
}

static void TestIni()
{
    CIniHelper ini(L"D:\\Temp\\config.ini");
    ini.RemoveSection(L"skin_360悬浮窗dark");
    ini.Save();
    int a = 0;
}

static void TestPluginVersion()
{
    ASSERT(PluginVersion(L"1.0.0") == PluginVersion(L"1.00"));
    ASSERT(PluginVersion(L"1.2") < PluginVersion(L"1.20"));
    ASSERT(PluginVersion(L"0.8.0") < PluginVersion(L"1.00"));
    ASSERT(PluginVersion(L"1.0.3") < PluginVersion(L"1.03"));

    //CPluginUpdateHelper helper;
    //helper.CheckForUpdate();
    //int a = 0;
}

static string MakeTrafficRecord(const SYSTEMTIME& date, unsigned __int64 up_k_bytes, unsigned __int64 down_k_bytes)
{
    char buffer[96];
    sprintf_s(buffer, "%.4u/%.2u/%.2u %llu/%llu",
        static_cast<unsigned int>(date.wYear), static_cast<unsigned int>(date.wMonth), static_cast<unsigned int>(date.wDay),
        up_k_bytes, down_k_bytes);
    return buffer;
}

static void WriteHistoryFixture(const wstring& file_path, const vector<string>& records, bool write_header)
{
    ofstream file{ file_path, std::ios::out | std::ios::trunc };
    ASSERT(file.is_open());
    if (write_header)
        file << "lines: \"" << records.size() << "\"\n";
    for (const string& record : records)
        file << record << "\n";
}

static vector<string> ReadHistoryFixture(const wstring& file_path)
{
    ifstream file{ file_path };
    vector<string> lines;
    string line;
    while (getline(file, line))
        lines.push_back(line);
    return lines;
}

static void WriteValidatedHistoryFixture(const wstring& file_path, const vector<string>& records)
{
    WriteHistoryFixture(file_path, records, true);
    CHistoryTrafficFile fixture(file_path);
    fixture.Load();
    ASSERT(fixture.Save(false));
    CHistoryTrafficFile validated(file_path);
    validated.Load();
    ASSERT(validated.IsSnapshotValid());
}

static void TestHistoryTrafficPersistence()
{
    wchar_t temp_directory[MAX_PATH]{};
    wchar_t temp_file[MAX_PATH]{};
    ASSERT(GetTempPathW(MAX_PATH, temp_directory) > 0);
    ASSERT(GetTempFileNameW(temp_directory, L"TMH", 0, temp_file) != 0);
    DeleteFileW(temp_file);

    const wstring file_path{ temp_file };
    const wstring checkpoint_path = file_path + L".checkpoint";
    const wstring backup_path = file_path + L".bak";
    auto cleanup = [&]() {
        for (const wstring& path : { file_path, file_path + L".tmp", checkpoint_path, checkpoint_path + L".tmp",
            backup_path, backup_path + L".tmp", backup_path + L".checkpoint" })
        {
            DeleteFileW(path.c_str());
        }
    };

    SYSTEMTIME today{};
    GetLocalTime(&today);
    FILETIME today_file_time{};
    ASSERT(SystemTimeToFileTime(&today, &today_file_time));
    ULARGE_INTEGER previous_day_value{};
    previous_day_value.LowPart = today_file_time.dwLowDateTime;
    previous_day_value.HighPart = today_file_time.dwHighDateTime;
    previous_day_value.QuadPart -= 24ull * 60 * 60 * 10000000;
    FILETIME previous_day_file_time{};
    previous_day_file_time.dwLowDateTime = previous_day_value.LowPart;
    previous_day_file_time.dwHighDateTime = previous_day_value.HighPart;
    SYSTEMTIME previous_day{};
    ASSERT(FileTimeToSystemTime(&previous_day_file_time, &previous_day));
    SYSTEMTIME older_day{};
    older_day.wYear = 2020;
    older_day.wMonth = 1;
    older_day.wDay = 2;

    WriteValidatedHistoryFixture(file_path,
        { MakeTrafficRecord(today, 100, 200), MakeTrafficRecord(previous_day, 5, 30) });
    WriteHistoryFixture(checkpoint_path, { MakeTrafficRecord(today, 120, 250) }, false);
    WriteValidatedHistoryFixture(backup_path,
        { MakeTrafficRecord(today, 150, 190), MakeTrafficRecord(previous_day, 10, 20),
            MakeTrafficRecord(older_day, 7, 9) });

    CHistoryTrafficFile traffic_file(file_path);
    ASSERT(traffic_file.Load());
    ASSERT(traffic_file.IsSnapshotValid());
    ASSERT(!traffic_file.IsBackupRecoveryRequired());
    ASSERT(!traffic_file.IsFullSaveRequiredAfterLoad());
    CHistoryTrafficFile backup_file(backup_path);
    backup_file.Load();
    ASSERT(traffic_file.Merge(backup_file, true) == 3);
    ASSERT(traffic_file.GetTodayTraffic().up_kBytes == 150);
    ASSERT(traffic_file.GetTodayTraffic().down_kBytes == 250);
    ASSERT(traffic_file.GetHistoryTraffics().size() == 2);
    const HistoryTraffic& previous_day_traffic = traffic_file.GetHistoryTraffics().front();
    ASSERT(previous_day_traffic.up_kBytes == 10);
    ASSERT(previous_day_traffic.down_kBytes == 30);
    const HistoryTraffic& older_day_traffic = traffic_file.GetHistoryTraffics().back();
    ASSERT(older_day_traffic.year == older_day.wYear);
    ASSERT(older_day_traffic.up_kBytes == 7);
    ASSERT(older_day_traffic.down_kBytes == 9);
    ASSERT(traffic_file.Save());
    ASSERT(CCommon::FileExist(backup_path.c_str()));

    CHistoryTrafficFile rotated_backup(backup_path);
    rotated_backup.Load();
    ASSERT(rotated_backup.IsSnapshotValid());
    ASSERT(rotated_backup.GetTodayTraffic().up_kBytes == 100);
    ASSERT(rotated_backup.GetTodayTraffic().down_kBytes == 200);

    CHistoryTrafficFile reloaded_file(file_path);
    ASSERT(!reloaded_file.Load());
    ASSERT(reloaded_file.IsSnapshotValid());
    ASSERT(reloaded_file.GetTodayTraffic().up_kBytes == 150);
    ASSERT(reloaded_file.GetTodayTraffic().down_kBytes == 250);

    DeleteFileW(checkpoint_path.c_str());
    DeleteFileW(backup_path.c_str());
    WriteHistoryFixture(file_path,
        { MakeTrafficRecord(today, 1, 2), "2024/02/31 3/4", "2024/01/01 invalid", "2024/01/01 18446744073709551616/1" }, false);
    CHistoryTrafficFile damaged_file(file_path);
    ASSERT(!damaged_file.Load());
    ASSERT(!damaged_file.IsSnapshotValid());
    ASSERT(damaged_file.IsBackupRecoveryRequired());
    ASSERT(damaged_file.IsFullSaveRequiredAfterLoad());
    ASSERT(damaged_file.GetTodayTraffic().up_kBytes == 1);
    ASSERT(damaged_file.GetTodayTraffic().down_kBytes == 2);
    ASSERT(damaged_file.GetHistoryTraffics().empty());

    WriteHistoryFixture(checkpoint_path, { MakeTrafficRecord(previous_day, 20, 40) }, false);
    CHistoryTrafficFile past_checkpoint_file(file_path);
    ASSERT(past_checkpoint_file.Load());
    ASSERT(past_checkpoint_file.IsFullSaveRequiredAfterLoad());

    DeleteFileW(checkpoint_path.c_str());
    WriteValidatedHistoryFixture(file_path,
        { MakeTrafficRecord(today, 11, 22), MakeTrafficRecord(previous_day, 33, 44) });
    vector<string> snapshot_lines = ReadHistoryFixture(file_path);
    ASSERT(snapshot_lines.size() == 4);

    vector<string> corrupted_lines = snapshot_lines;
    corrupted_lines[1] = MakeTrafficRecord(today, 12, 22);
    WriteHistoryFixture(file_path, corrupted_lines, false);
    CHistoryTrafficFile corrupted_snapshot(file_path);
    corrupted_snapshot.Load();
    ASSERT(!corrupted_snapshot.IsSnapshotValid());

    vector<string> wrong_count_lines = snapshot_lines;
    wrong_count_lines[0] = "lines: \"3\"";
    WriteHistoryFixture(file_path, wrong_count_lines, false);
    CHistoryTrafficFile wrong_count_snapshot(file_path);
    wrong_count_snapshot.Load();
    ASSERT(!wrong_count_snapshot.IsSnapshotValid());

    vector<string> wrong_checksum_lines = snapshot_lines;
    wrong_checksum_lines.back() = "checksum: \"fnv1a64:0000000000000000\"";
    WriteHistoryFixture(file_path, wrong_checksum_lines, false);
    CHistoryTrafficFile wrong_checksum_snapshot(file_path);
    wrong_checksum_snapshot.Load();
    ASSERT(!wrong_checksum_snapshot.IsSnapshotValid());

    vector<string> trailing_data_lines = snapshot_lines;
    trailing_data_lines.push_back(MakeTrafficRecord(older_day, 1, 1));
    WriteHistoryFixture(file_path, trailing_data_lines, false);
    CHistoryTrafficFile trailing_data_snapshot(file_path);
    trailing_data_snapshot.Load();
    ASSERT(!trailing_data_snapshot.IsSnapshotValid());

    cleanup();
}

static void TestHistoryTrafficCheckpointSchedule()
{
    CHistoryTrafficCheckpointSchedule schedule;
    schedule.Reset(100, 1000);

    ASSERT(!schedule.ShouldSave(100, 1000 + CHistoryTrafficCheckpointSchedule::MAX_INTERVAL_MS));
    ASSERT(!schedule.ShouldSave(101, 1000 + CHistoryTrafficCheckpointSchedule::MIN_INTERVAL_MS));
    ASSERT(schedule.ShouldSave(101, 1000 + CHistoryTrafficCheckpointSchedule::MAX_INTERVAL_MS));

    schedule.MarkSaved(101, 1000 + CHistoryTrafficCheckpointSchedule::MAX_INTERVAL_MS);
    const ULONGLONG saved_tick = 1000 + CHistoryTrafficCheckpointSchedule::MAX_INTERVAL_MS;
    const unsigned __int64 threshold_total = 101 + CHistoryTrafficCheckpointSchedule::TRAFFIC_THRESHOLD_KBYTES;
    ASSERT(!schedule.ShouldSave(threshold_total,
        saved_tick + CHistoryTrafficCheckpointSchedule::MIN_INTERVAL_MS - 1));
    ASSERT(schedule.ShouldSave(threshold_total,
        saved_tick + CHistoryTrafficCheckpointSchedule::MIN_INTERVAL_MS));

    schedule.Reset(500, 200000);
    ASSERT(!schedule.ShouldSave(500, 100));
    ASSERT(schedule.ShouldSave(501, 100));
}

static void TestHistoryTrafficFullSaveRetrySchedule()
{
    CHistoryTrafficFullSaveRetrySchedule schedule;
    ULONGLONG attempt_tick = 1000;
    schedule.Reset(attempt_tick);

    ASSERT(schedule.GetRetryInterval() == 15ull * 1000);
    ASSERT(!schedule.ShouldRetry(attempt_tick + schedule.GetRetryInterval() - 1));

    const ULONGLONG expected_intervals[] = {
        30ull * 1000,
        60ull * 1000,
        120ull * 1000,
        240ull * 1000,
        300ull * 1000,
        300ull * 1000
    };
    for (ULONGLONG expected_interval : expected_intervals)
    {
        attempt_tick += schedule.GetRetryInterval();
        ASSERT(schedule.ShouldRetry(attempt_tick));
        schedule.MarkFailed(attempt_tick);
        ASSERT(schedule.GetRetryInterval() == expected_interval);
    }

    schedule.Reset(attempt_tick);
    ASSERT(schedule.GetRetryInterval() == CHistoryTrafficFullSaveRetrySchedule::INITIAL_INTERVAL_MS);
    ASSERT(schedule.ShouldRetry(0));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTest::CTest()
{
}

CTest::~CTest()
{
}

void CTest::Test()
{
    //TestHttpQequest();
    //TestGetLicense();
    //TestSkin();
    //TestCrash();
    //TestDate();
    //TestIni();
    TestPluginVersion();
    TestHistoryTrafficPersistence();
    TestHistoryTrafficCheckpointSchedule();
    TestHistoryTrafficFullSaveRetrySchedule();
}

void CTest::TestCommand()
{
    //TestPlugin();

    //测试消息对话框
    CMessageDlg dlg;
    dlg.SetWindowTitle(_T("System Info"));
    dlg.SetInfoText(_T("System Information for TrafficMonitor."));
    dlg.SetMessageText(theApp.GetSystemInfoString().GetString());
    dlg.SetStandarnMessageIcon(CMessageDlg::SI_INFORMATION);
    dlg.DoModal();
}
