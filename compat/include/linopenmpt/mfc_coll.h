// linOpenMPT compatibility layer: MFC collection templates (CList, CMap).
// Positions are 1-based indices, so that nullptr ends an iteration. CMap
// keeps insertion order; MFC's order is unspecified, so callers do not
// depend on it.
#pragma once

#include <cstddef>
#include <utility>
#include <vector>

template <typename TYPE, typename ARG_TYPE = const TYPE &>
class CList : public CObject
{
public:
	POSITION AddHead(ARG_TYPE item) { m_items.insert(m_items.begin(), TYPE(item)); return ToPos(0); }
	POSITION AddTail(ARG_TYPE item) { m_items.push_back(TYPE(item)); return ToPos(m_items.size() - 1); }
	void RemoveAll() { m_items.clear(); }
	INT_PTR GetCount() const { return static_cast<INT_PTR>(m_items.size()); }
	INT_PTR GetSize() const { return GetCount(); }
	BOOL IsEmpty() const { return m_items.empty(); }
	TYPE &GetHead() { return m_items.front(); }
	const TYPE &GetHead() const { return m_items.front(); }
	TYPE &GetTail() { return m_items.back(); }
	const TYPE &GetTail() const { return m_items.back(); }
	POSITION GetHeadPosition() const { return m_items.empty() ? nullptr : ToPos(0); }
	POSITION GetTailPosition() const { return m_items.empty() ? nullptr : ToPos(m_items.size() - 1); }
	TYPE &GetNext(POSITION &pos)
	{
		const std::size_t i = FromPos(pos);
		pos = (i + 1 < m_items.size()) ? ToPos(i + 1) : nullptr;
		return m_items[i];
	}
	const TYPE &GetNext(POSITION &pos) const { return const_cast<CList *>(this)->GetNext(pos); }
	TYPE &GetPrev(POSITION &pos)
	{
		const std::size_t i = FromPos(pos);
		pos = (i > 0) ? ToPos(i - 1) : nullptr;
		return m_items[i];
	}
	TYPE &GetAt(POSITION pos) { return m_items[FromPos(pos)]; }
	const TYPE &GetAt(POSITION pos) const { return m_items[FromPos(pos)]; }

private:
	static POSITION ToPos(std::size_t i) { return reinterpret_cast<POSITION>(static_cast<UINT_PTR>(i + 1)); }
	static std::size_t FromPos(POSITION pos) { return static_cast<std::size_t>(reinterpret_cast<UINT_PTR>(pos) - 1); }
	std::vector<TYPE> m_items;
};

template <typename KEY, typename ARG_KEY, typename VALUE, typename ARG_VALUE>
class CMap : public CObject
{
public:
	VALUE &operator[](ARG_KEY key)
	{
		if(auto i = Find(key); i != npos)
			return m_items[i].second;
		m_items.emplace_back(KEY(key), VALUE{});
		return m_items.back().second;
	}
	BOOL Lookup(ARG_KEY key, VALUE &value) const
	{
		if(auto i = Find(key); i != npos)
		{
			value = m_items[i].second;
			return TRUE;
		}
		return FALSE;
	}
	void SetAt(ARG_KEY key, ARG_VALUE value) { (*this)[key] = value; }
	BOOL RemoveKey(ARG_KEY key)
	{
		if(auto i = Find(key); i != npos)
		{
			m_items.erase(m_items.begin() + i);
			return TRUE;
		}
		return FALSE;
	}
	void RemoveAll() { m_items.clear(); }
	INT_PTR GetCount() const { return static_cast<INT_PTR>(m_items.size()); }
	INT_PTR GetSize() const { return GetCount(); }
	BOOL IsEmpty() const { return m_items.empty(); }
	// Positions are 1-based indices, so that nullptr ends the iteration.
	POSITION GetStartPosition() const { return m_items.empty() ? nullptr : reinterpret_cast<POSITION>(static_cast<UINT_PTR>(1)); }
	void GetNextAssoc(POSITION &pos, KEY &key, VALUE &value) const
	{
		const auto i = reinterpret_cast<UINT_PTR>(pos) - 1;
		key = m_items[i].first;
		value = m_items[i].second;
		pos = (i + 1 < m_items.size()) ? reinterpret_cast<POSITION>(static_cast<UINT_PTR>(i + 2)) : nullptr;
	}

private:
	static constexpr std::size_t npos = static_cast<std::size_t>(-1);
	std::size_t Find(ARG_KEY key) const
	{
		for(std::size_t i = 0; i < m_items.size(); i++)
		{
			if(m_items[i].first == key)
				return i;
		}
		return npos;
	}
	std::vector<std::pair<KEY, VALUE>> m_items;
};
