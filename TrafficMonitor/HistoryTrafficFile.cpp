#include "stdafx.h"
#include "HistoryTrafficFile.h"
#include "Common.h"
#include <limits>

namespace
{
	bool HasTraffic(const HistoryTraffic& traffic)
	{
		return traffic.up_kBytes != 0 || traffic.down_kBytes != 0;
	}

	unsigned __int64 SaturatedAdd(unsigned __int64 left, unsigned __int64 right)
	{
		const unsigned __int64 max_value = (std::numeric_limits<unsigned __int64>::max)();
		return left > max_value - right ? max_value : left + right;
	}

	unsigned __int64 KBytesToBytes(unsigned __int64 k_bytes)
	{
		const unsigned __int64 max_value = (std::numeric_limits<unsigned __int64>::max)();
		return k_bytes > max_value / 1024 ? max_value : k_bytes * 1024;
	}

	bool ParseUnsigned64(const string& text, unsigned __int64& value)
	{
		if (text.empty())
			return false;

		const unsigned __int64 max_value = (std::numeric_limits<unsigned __int64>::max)();
		unsigned __int64 parsed{};
		for (char ch : text)
		{
			if (ch < '0' || ch > '9')
				return false;
			unsigned int digit = static_cast<unsigned int>(ch - '0');
			if (parsed > (max_value - digit) / 10)
				return false;
			parsed = parsed * 10 + digit;
		}
		value = parsed;
		return true;
	}

	template<typename Writer>
	bool WriteFileAtomically(const wstring& file_path, Writer writer, bool durable, const wstring& backup_path = L"")
	{
		const wstring temp_path = file_path + L".tmp";
		DeleteFileW(temp_path.c_str());

		ofstream file{ temp_path, std::ios::out | std::ios::trunc };
		if (!file.is_open())
			return false;

		writer(file);
		file.flush();
		bool success = file.good();
		file.close();
		success = success && !file.fail();
		if (!success)
		{
			DeleteFileW(temp_path.c_str());
			return false;
		}

		if (durable)
		{
			HANDLE file_handle = CreateFileW(temp_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file_handle == INVALID_HANDLE_VALUE)
			{
				DeleteFileW(temp_path.c_str());
				return false;
			}
			success = (FlushFileBuffers(file_handle) != FALSE);
			CloseHandle(file_handle);
			if (!success)
			{
				DeleteFileW(temp_path.c_str());
				return false;
			}
		}

		DWORD move_flags = MOVEFILE_REPLACE_EXISTING;
		if (durable)
			move_flags |= MOVEFILE_WRITE_THROUGH;
		if (!backup_path.empty() && CCommon::FileExist(file_path.c_str())
			&& !MoveFileExW(file_path.c_str(), backup_path.c_str(), move_flags))
		{
			DeleteFileW(temp_path.c_str());
			return false;
		}
		if (!MoveFileExW(temp_path.c_str(), file_path.c_str(), move_flags))
		{
			DeleteFileW(temp_path.c_str());
			return false;
		}
		return true;
	}
}

void CHistoryTrafficCheckpointSchedule::Reset(unsigned __int64 current_kbytes, ULONGLONG current_tick)
{
	m_last_saved_kbytes = current_kbytes;
	m_last_saved_tick = current_tick;
	m_initialized = true;
}

bool CHistoryTrafficCheckpointSchedule::ShouldSave(unsigned __int64 current_kbytes, ULONGLONG current_tick) const
{
	if (!m_initialized || current_kbytes == m_last_saved_kbytes)
		return false;

	const ULONGLONG elapsed = current_tick >= m_last_saved_tick
		? current_tick - m_last_saved_tick
		: MAX_INTERVAL_MS;
	if (elapsed >= MAX_INTERVAL_MS)
		return true;

	const unsigned __int64 delta = current_kbytes >= m_last_saved_kbytes
		? current_kbytes - m_last_saved_kbytes
		: (std::numeric_limits<unsigned __int64>::max)();
	return elapsed >= MIN_INTERVAL_MS && delta >= TRAFFIC_THRESHOLD_KBYTES;
}

void CHistoryTrafficCheckpointSchedule::MarkSaved(unsigned __int64 current_kbytes, ULONGLONG current_tick)
{
	Reset(current_kbytes, current_tick);
}

CHistoryTrafficFile::CHistoryTrafficFile(const wstring& file_path)
	: m_file_path(file_path)
{
}

CHistoryTrafficFile::~CHistoryTrafficFile()
{
}

HistoryTraffic CHistoryTrafficFile::CreateTodayTraffic() const
{
	SYSTEMTIME current_time;
	GetLocalTime(&current_time);
	HistoryTraffic today_traffic;
	today_traffic.year = current_time.wYear;
	today_traffic.month = current_time.wMonth;
	today_traffic.day = current_time.wDay;
	today_traffic.up_kBytes = 0;
	today_traffic.down_kBytes = 0;
	today_traffic.mixed = false;
	return today_traffic;
}

void CHistoryTrafficFile::WriteTrafficRecord(ofstream& file, const HistoryTraffic& traffic) const
{
	char buff[64];
	if (traffic.mixed)
	{
		sprintf_s(buff, "%.4d/%.2d/%.2d %llu", traffic.year, traffic.month, 
			traffic.day, traffic.down_kBytes);
	}
	else
	{
		sprintf_s(buff, "%.4d/%.2d/%.2d %llu/%llu", traffic.year, traffic.month, 
			traffic.day, traffic.up_kBytes, traffic.down_kBytes);
	}
	file << buff << "\n";
}

void CHistoryTrafficFile::UpdateCache() const
{
	// 更新缓存：合并今天的记录和历史记录链表
	m_traffics_cache.clear();
	m_traffics_cache.push_front(m_today_traffic);
	m_traffics_cache.insert(m_traffics_cache.end(), m_history_traffics.begin(), m_history_traffics.end());
	m_cache_dirty = false; // 标记缓存已更新
}

bool CHistoryTrafficFile::SaveToFile(const wstring& file_path, const wstring& backup_path) const
{
	return WriteFileAtomically(file_path, [this](ofstream& file) {
		char buff[64];
		size_t total_size = 1 + m_history_traffics.size();
		sprintf_s(buff, "lines: \"%u\"", static_cast<unsigned int>(total_size));
		file << buff << "\n";
		WriteTrafficRecord(file, m_today_traffic);
		for (const auto& history_traffic : m_history_traffics)
			WriteTrafficRecord(file, history_traffic);
	}, true, backup_path);
}

bool CHistoryTrafficFile::Save(bool rotate_backup) const
{
	return SaveToFile(m_file_path, rotate_backup ? GetBackupPath() : L"");
}

bool CHistoryTrafficFile::IsTodayRecord() const
{
	// 检查今天的记录日期是否正确
	SYSTEMTIME current_time;
	GetLocalTime(&current_time);
	
	return (m_today_traffic.year == current_time.wYear &&
			m_today_traffic.month == current_time.wMonth &&
			m_today_traffic.day == current_time.wDay);
}

bool CHistoryTrafficFile::SaveTodayOnly(bool durable) const
{
	if (!IsTodayRecord())
		return false;

	return WriteFileAtomically(GetCheckpointPath(), [this](ofstream& file) {
		WriteTrafficRecord(file, m_today_traffic);
	}, durable);
}

bool CHistoryTrafficFile::ParseTrafficRecord(const string& line, HistoryTraffic& traffic) const
{
	if (line.size() < 12 || line[4] != '/' || line[7] != '/' || line[10] != ' ')
		return false;
	for (size_t index : { 0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u })
	{
		if (line[index] < '0' || line[index] > '9')
			return false;
	}

	HistoryTraffic parsed{};
	parsed.year = atoi(line.substr(0, 4).c_str());
	parsed.month = atoi(line.substr(5, 2).c_str());
	parsed.day = atoi(line.substr(8, 2).c_str());
	SYSTEMTIME record_date{};
	record_date.wYear = static_cast<WORD>(parsed.year);
	record_date.wMonth = static_cast<WORD>(parsed.month);
	record_date.wDay = static_cast<WORD>(parsed.day);
	FILETIME ignored_file_time{};
	if (parsed.year < 1900 || parsed.year > 3000 || !::SystemTimeToFileTime(&record_date, &ignored_file_time))
		return false;

	size_t separator_index = line.find('/', 11);
	parsed.mixed = (separator_index == string::npos);
	if (parsed.mixed)
	{
		if (!ParseUnsigned64(line.substr(11), parsed.down_kBytes))
			return false;
		parsed.up_kBytes = 0;
	}
	else
	{
		if (line.find('/', separator_index + 1) != string::npos
			|| !ParseUnsigned64(line.substr(11, separator_index - 11), parsed.up_kBytes)
			|| !ParseUnsigned64(line.substr(separator_index + 1), parsed.down_kBytes))
			return false;
	}

	traffic = parsed;
	return true;
}

bool CHistoryTrafficFile::RecoverFromCheckpoint()
{
	ifstream file{ GetCheckpointPath() };
	string line;
	HistoryTraffic checkpoint;
	if (!file.is_open() || !getline(file, line) || !ParseTrafficRecord(line, checkpoint) || !HasTraffic(checkpoint))
		return false;

	HistoryTraffic today = CreateTodayTraffic();
	if (HistoryTraffic::DateGreater(checkpoint, today))
		return false;

	HistoryTraffic* target = nullptr;
	bool recovered = false;
	if (HistoryTraffic::DateEqual(checkpoint, m_today_traffic))
	{
		target = &m_today_traffic;
	}
	else
	{
		auto iter = std::find_if(m_history_traffics.begin(), m_history_traffics.end(),
			[&checkpoint](const HistoryTraffic& item) { return HistoryTraffic::DateEqual(item, checkpoint); });
		if (iter == m_history_traffics.end())
		{
			m_history_traffics.push_back(checkpoint);
			recovered = true;
		}
		else
			target = &(*iter);
	}

	if (target != nullptr)
		recovered = MergeTrafficRecord(*target, checkpoint, true);
	if (recovered && !HistoryTraffic::DateEqual(checkpoint, today))
		m_checkpoint_full_save_required = true;
	return recovered;
}

bool CHistoryTrafficFile::Load()
{
	m_today_traffic = HistoryTraffic{}; // 初始化今天的记录
	m_history_traffics.clear(); // 清空历史记录链表
	m_checkpoint_full_save_required = false;
	InvalidateCache(); // 标记缓存过期

	ifstream file{ m_file_path };
	string current_line;
	HistoryTraffic traffic;
	bool is_first_data_line = true; // 标记是否是第一条数据行（今天的记录）
	auto load_record = [&](const string& line)
	{
		if (ParseTrafficRecord(line, traffic) && HasTraffic(traffic))
		{
			if (is_first_data_line)
			{
				m_today_traffic = traffic;
				is_first_data_line = false;
			}
			else
				m_history_traffics.push_back(traffic);
		}
	};

	if (file.is_open() && getline(file, current_line))
	{
		//兼容缺失或损坏的lines头：首行本身是记录时仍然尝试恢复
		if (current_line.find("lines:") != 0)
			load_record(current_line);
		while (getline(file, current_line))
			load_record(current_line);
	}
	file.close();

	MormalizeData();
	bool checkpoint_recovered = RecoverFromCheckpoint();
	if (checkpoint_recovered)
	{
		MormalizeData();
	}
	return checkpoint_recovered;
}

bool CHistoryTrafficFile::MergeTrafficRecord(HistoryTraffic& target, const HistoryTraffic& source, bool prefer_larger_value) const
{
	const unsigned __int64 old_up = target.up_kBytes;
	const unsigned __int64 old_down = target.down_kBytes;
	const bool old_mixed = target.mixed;
	if (prefer_larger_value)
	{
		target.up_kBytes = (std::max)(target.up_kBytes, source.up_kBytes);
		target.down_kBytes = (std::max)(target.down_kBytes, source.down_kBytes);
	}
	else
	{
		target.up_kBytes = SaturatedAdd(target.up_kBytes, source.up_kBytes);
		target.down_kBytes = SaturatedAdd(target.down_kBytes, source.down_kBytes);
	}
	target.mixed = target.mixed && source.mixed;
	return target.up_kBytes != old_up || target.down_kBytes != old_down || target.mixed != old_mixed;
}

size_t CHistoryTrafficFile::Merge(const CHistoryTrafficFile& history_traffic, bool prefer_larger_value)
{
	size_t changed_records{};
	HistoryTraffic today_traffic = CreateTodayTraffic();
	if (HistoryTraffic::DateEqual(m_today_traffic, history_traffic.m_today_traffic)
		&& MergeTrafficRecord(m_today_traffic, history_traffic.m_today_traffic, prefer_larger_value))
	{
		++changed_records;
	}

	for (const HistoryTraffic& traffic : history_traffic.m_history_traffics)
	{
		if (HistoryTraffic::DateGreater(traffic, today_traffic))
			continue;

		auto iter = std::find_if(m_history_traffics.begin(), m_history_traffics.end(),
			[&traffic](const HistoryTraffic& existing) { return HistoryTraffic::DateEqual(existing, traffic); });
		if (iter == m_history_traffics.end())
		{
			m_history_traffics.push_back(traffic);
			++changed_records;
		}
		else if (MergeTrafficRecord(*iter, traffic, prefer_larger_value))
			++changed_records;
	}

	if (changed_records > 0)
		MormalizeData();
	return changed_records;
}

void CHistoryTrafficFile::OnDateChanged()
{
	// 日期改变时，将今天的记录移到历史记录链表的前面，然后创建新的今天的记录
	
	// 如果今天的记录有数据，将其移到历史记录链表
	if (HasTraffic(m_today_traffic))
	{
		m_history_traffics.push_front(m_today_traffic);
		// 立即排序，确保数据一致性（按日期从大到小）
		if (m_history_traffics.size() >= 2)
		{
			m_history_traffics.sort(HistoryTraffic::DateGreater);
		}
	}
	
	// 创建新的今天的记录
	m_today_traffic = CreateTodayTraffic();
	
	// 更新统计
	m_today_up_traffic = 0;
	m_today_down_traffic = 0;
	m_size = 1 + m_history_traffics.size();
	InvalidateCache(); // 标记缓存过期
}

void CHistoryTrafficFile::MormalizeData()
{
	HistoryTraffic today_traffic = CreateTodayTraffic();

	// 先对历史记录链表排序（按日期从大到小），以便后续查找和合并
	if (m_history_traffics.size() >= 2)
	{
		m_history_traffics.sort(HistoryTraffic::DateGreater);

		// 合并相同日期的记录
		auto it = m_history_traffics.begin();
		while (it != m_history_traffics.end())
		{
			auto next_it = it;
			++next_it;
			if (next_it != m_history_traffics.end() && HistoryTraffic::DateEqual(*it, *next_it))
			{
				it->up_kBytes = SaturatedAdd(it->up_kBytes, next_it->up_kBytes);
				it->down_kBytes = SaturatedAdd(it->down_kBytes, next_it->down_kBytes);
				it->mixed = it->mixed && next_it->mixed;
				m_history_traffics.erase(next_it);
			}
			else
			{
				++it;
			}
		}
	}

	// 清理日期晚于当前日期的历史记录（系统时间可能被调整了）
	// 历史记录应该都是过去的日期，不应该有未来的日期
	if (!m_history_traffics.empty())
	{
		auto it = m_history_traffics.begin();
		while (it != m_history_traffics.end())
		{
			// 如果历史记录的日期晚于今天，说明是"未来"的记录，应该删除
			if (HistoryTraffic::DateGreater(*it, today_traffic))
			{
				it = m_history_traffics.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	// 如果 m_today_traffic 的日期也晚于当前日期，说明系统时间被调整了，应该重置
	if (HistoryTraffic::DateGreater(m_today_traffic, today_traffic))
	{
		// 如果今天的记录有数据，应该将其移到历史记录（但日期晚于今天，会被上面的清理逻辑删除）
		// 直接重置为今天的记录
		m_today_traffic = today_traffic;
	}

	// 在历史记录中查找今天的记录（可能历史记录中包含了今天的数据）
	auto it = std::find_if(m_history_traffics.begin(), m_history_traffics.end(),
		[&today_traffic](const HistoryTraffic& traffic) {
			return HistoryTraffic::DateEqual(traffic, today_traffic);
		});

	if (it != m_history_traffics.end())
	{
		// 历史记录中找到了今天的记录
		if (HistoryTraffic::DateEqual(m_today_traffic, today_traffic))
		{
			// 如果 m_today_traffic 也是今天的，合并数据（避免数据丢失）
			m_today_traffic.up_kBytes = SaturatedAdd(m_today_traffic.up_kBytes, it->up_kBytes);
			m_today_traffic.down_kBytes = SaturatedAdd(m_today_traffic.down_kBytes, it->down_kBytes);
		}
		else
		{
			// 如果 m_today_traffic 不是今天的，用历史记录中的替换
			m_today_traffic = *it;
		}
		// 从历史记录中删除今天的记录（因为应该只在 m_today_traffic 中）
		m_history_traffics.erase(it);
	}
	else if (!HistoryTraffic::DateEqual(m_today_traffic, today_traffic))
	{
		// 历史记录中没有今天的记录，且 m_today_traffic 也不是今天的
		// 如果 m_today_traffic 有数据，应该将其移到历史记录链表
		if (HasTraffic(m_today_traffic))
		{
			m_history_traffics.push_front(m_today_traffic);
			// 重新排序（因为插入了新记录）
			if (m_history_traffics.size() >= 2)
			{
				m_history_traffics.sort(HistoryTraffic::DateGreater);
			}
		}
		// 创建新的今天的记录
		m_today_traffic = today_traffic;
	}

	// 更新今天的流量统计
	m_today_up_traffic = KBytesToBytes(m_today_traffic.up_kBytes);
	m_today_down_traffic = KBytesToBytes(m_today_traffic.down_kBytes);
	m_today_traffic.mixed = false;

	// 更新总记录数
	m_size = 1 + m_history_traffics.size();
	InvalidateCache(); // 标记缓存过期
}
