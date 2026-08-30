#pragma once
#include "CommonData.h"

class CHistoryTrafficCheckpointSchedule
{
public:
	static constexpr ULONGLONG MIN_INTERVAL_MS = 15ull * 1000;
	static constexpr ULONGLONG MAX_INTERVAL_MS = 60ull * 1000;
	static constexpr unsigned __int64 TRAFFIC_THRESHOLD_KBYTES = 4ull * 1024;

	void Reset(unsigned __int64 current_kbytes, ULONGLONG current_tick);
	bool ShouldSave(unsigned __int64 current_kbytes, ULONGLONG current_tick) const;
	void MarkSaved(unsigned __int64 current_kbytes, ULONGLONG current_tick);

private:
	unsigned __int64 m_last_saved_kbytes{};
	ULONGLONG m_last_saved_tick{};
	bool m_initialized{};
};

class CHistoryTrafficFile
{
public:
	CHistoryTrafficFile(const wstring& file_path);
	~CHistoryTrafficFile();

	bool Save(bool rotate_backup = true) const;
	bool SaveTodayOnly(bool durable = false) const;	// 将当天记录保存到小型检查点文件
	bool Load();			//返回检查点是否恢复了比主文件更新的数据
	bool IsFullSaveRequiredAfterLoad() const { return m_checkpoint_full_save_required || m_snapshot_rewrite_required || !m_snapshot_valid; }
	bool IsBackupRecoveryRequired() const { return !m_snapshot_valid; }
	bool IsSnapshotValid() const { return m_snapshot_valid; }
	size_t Merge(const CHistoryTrafficFile& history_traffic, bool prefer_larger_value = false);
	size_t RestoreFromValidatedSnapshot(const CHistoryTrafficFile& history_traffic);
	void OnDateChanged();		//日期改变时调用，将今天的记录移到历史记录，创建新的今天的记录

	const wstring& GetFilePath() const { return m_file_path; }
	const void SetFilePath(const wstring& file_path) { m_file_path = file_path; }
	
	// 获取今天的记录（第一条）
	HistoryTraffic& GetTodayTraffic() { return m_today_traffic; }
	const HistoryTraffic& GetTodayTraffic() const { return m_today_traffic; }
	
	// 获取历史记录链表
	const list<HistoryTraffic>& GetHistoryTraffics() const { return m_history_traffics; }
	
	unsigned __int64 GetTodayUpTraffic() const { return m_today_up_traffic; }
	unsigned __int64 GetTodayDownTraffic() const { return m_today_down_traffic; }
	size_t Size() const { return m_size; }

private:
	void MormalizeData();		//将历史流量数据排序并合并相同项
	void RefreshDerivedData();	//更新当天字节数和总记录数
	bool IsTodayRecord() const;	//检查今天的记录日期是否正确
	bool SaveToFile(const wstring& file_path, const wstring& backup_path = L"") const;
	void WriteTrafficRecord(ofstream& file, const HistoryTraffic& traffic, unsigned __int64* checksum = nullptr) const;
	bool ParseTrafficRecord(const string& line, HistoryTraffic& traffic) const;
	bool MergeTrafficRecord(HistoryTraffic& target, const HistoryTraffic& source, bool prefer_larger_value) const;
	bool RecoverFromCheckpoint();
	wstring GetCheckpointPath() const { return m_file_path + L".checkpoint"; }
	wstring GetBackupPath() const { return m_file_path + L".bak"; }
	HistoryTraffic CreateTodayTraffic() const;	//创建今天的记录（日期为当前日期，流量为0）

private:
	wstring m_file_path;
	HistoryTraffic m_today_traffic;        // 今天的记录（单独存储，频繁更新）
	list<HistoryTraffic> m_history_traffics;	// 历史记录链表（按日期从大到小排序）
	bool m_checkpoint_full_save_required{};
	bool m_snapshot_valid{};
	bool m_snapshot_rewrite_required{};
	unsigned __int64 m_today_up_traffic{};	//今天已使用的上传流量
	unsigned __int64 m_today_down_traffic{};	//今天已使用的下载流量
	size_t m_size{};				//流量数据的数量
};

