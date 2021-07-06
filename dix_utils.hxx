#pragma once
#include <string>
#include <utility>
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <filesystem>
using namespace std::filesystem;

#define _X_TRACE(_X_, ...) printf(_X_, __VA_ARGS__)
#define _X_PANIC(_X_, ...) do {\
								printf("file:[%s] line:%d\r\n", __FILE__, __LINE__);\
								printf(_X_, __VA_ARGS__);\
							}while(false)
#define _X_ASSERT(_X_) _ASSERTE(_X_)

#define FG_BLACK               0x00
#define FG_BLUE                0x01
#define FG_GREEN               0x02
#define FG_BLUE_2              0x03
#define FG_RED                 0x04
#define FG_PURPLE              0x05
#define FG_YELLOW              0x06
#define FG_WHITE               0x0F

inline void setColor(int color, const bool bIntensity = true) {
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color | (bIntensity ? FOREGROUND_INTENSITY : 0));
}
inline void resetColor() {
	setColor(FG_WHITE);
}
inline int color_printf(const int color, const char* pszFmt, ...) {
	if (color != -1) {
		setColor(color);
	}
	va_list  marker;
	va_start(marker, pszFmt);
	auto ri = vprintf(pszFmt, marker);
	va_end(marker);
	resetColor();
	return ri;
}

std::string inline ltrim(const std::string& s, const std::string& tokens)
{
	if (tokens.empty() || s.empty())
	{
		return s;
	}

	size_t trim_size = 0;
	for (size_t i = 0; i < s.length(); i++)
	{
		if (tokens.find(s[i]) != tokens.npos)
		{
			trim_size++;
		}
		else
		{
			break;
		}
	}

	if (trim_size == 0)
	{
		return s;
	}
	return s.substr(trim_size);
}

std::string inline rtrim(const std::string& s, const std::string& tokens)
{
	if (tokens.empty() || s.empty())
	{
		return s;
	}

	size_t trim_size = 0;
	for (size_t i = s.length() - 1; i >= 0; i--)
	{
		if (tokens.find(s[i]) != tokens.npos)
		{
			trim_size++;
		}
		else
		{
			break;
		}
	}

	if (trim_size == 0)
	{
		return s;
	}
	return s.substr(0, s.length() - trim_size);
}

std::string inline trim(const std::string& s, const std::string&& tokens)
{
	return ltrim(rtrim(s, tokens), tokens);
}

inline int get_filesize(FILE* file)
{
	fseek(file, 0, SEEK_END);
	int len = ftell(file);
	fseek(file, 0, SEEK_SET);
	return len;
}

inline int get_filesize(const char* fileName)
{
	return (int)std::filesystem::file_size(fileName);
}

class PIX
{
protected:
	using UCHAR = unsigned char;
	using PUCHAR = UCHAR*;

	PUCHAR m_ptr;
	size_t m_size;
	size_t m_cap;
public:
	static constexpr size_t npos = -1;
	PIX() noexcept
		:m_ptr{}
		,m_size{}
		,m_cap{}
	{}
	PIX(const PIX& _other)
		:m_size{ _other.m_size }
		, m_cap{ _other.m_cap }
	{
		if (m_cap)
		{
			m_ptr = new UCHAR[m_cap + 1];
			memcpy(m_ptr, _other.m_ptr, m_size);
		}
		else
		{
			m_ptr = nullptr;
		}
	}
	PIX(PIX&& _other)
		:m_ptr{ std::exchange(_other.m_ptr, nullptr) }
		, m_size{ std::exchange(_other.m_size, 0) }
		, m_cap{ std::exchange(_other.m_cap, 0) }
	{}
	PIX(const void* pdata, const size_t isize)
		: m_cap{isize}
		,m_size{isize}
	{
		if (isize)
		{
			m_ptr = new UCHAR[isize + 1];
			memcpy(m_ptr, pdata, isize);
		}
		else
		{
			m_ptr = nullptr;
		}
	}
	PIX(const char* str)
		:PIX{str, strlen(str)}
	{}
	PIX(const string& s)
		:PIX{s.c_str(), s.size()}
	{}
	~PIX() noexcept
	{
		cleanup();
	}
	PIX& operator= (const PIX& _right)
	{
		assert(&_right != this);
		PIX temp{_right};
		
		m_ptr  = std::exchange(temp.m_ptr, m_ptr);
		m_size = std::exchange(temp.m_size, m_size);
		m_cap  = std::exchange(temp.m_cap, m_cap);

		return *this;
	}
	PIX& operator= (PIX&& _right)
	{ 
		assert(&_right != this);

		m_ptr  = std::exchange(_right.m_ptr, m_ptr);
		m_size = std::exchange(_right.m_size, m_size);
		m_cap  = std::exchange(_right.m_cap, m_cap);
		_right.cleanup();

		return *this;
	}
	PIX& operator= (const string& s)
	{
		PIX temp{s};
		std::swap(*this, temp);
		return *this;
	}
	PIX& operator= (const char* str)
	{
		PIX temp{str};
		std::swap(*this, temp);
		return *this;
	}
	PIX& operator+= (const char* str)
	{
		return append(str, strlen(str));
	}
	PIX& operator+= (const string& s)
	{
		return append(s.c_str(), s.size());
	}
	PIX& operator+= (const PIX& _right)
	{
		return append(_right.data(), _right.size());
	}
	PIX& operator<< (const char* str)
	{
		return append(str, strlen(str));
	}
	PIX& operator<< (const string& s)
	{
		return append(s.c_str(), s.size());
	}
	PIX& operator<< (const PIX& _right)
	{
		return append(_right.data(), _right.size());
	}
	PIX operator+ (const char* str)
	{
		PIX temp{*this};
		temp.append(str, strlen(str));
		return std::move(temp);
	}
	PIX operator+ (const string& s)
	{
		PIX temp{ *this };
		temp.append(s.c_str(), s.size());
		return std::move(temp);
	}
	PIX operator+ (const PIX& _right)
	{
		PIX temp{ *this };
		temp.append(_right.data(), _right.size());
		return std::move(temp);
	}

	size_t size() const noexcept
	{
		return m_size;
	}
	size_t cap() const noexcept
	{
		return m_cap;
	}
	size_t unused() const noexcept
	{
		return cap() - size();
	}
	PUCHAR data() const noexcept
	{
		return m_ptr;
	}
	PUCHAR data(const size_t offIdx)
	{
		assert(offIdx <= m_size);
		return data() + offIdx;
	}

	const char* c_str()
	{
		if (m_ptr == nullptr)
		{
			return nullptr;
		}
		m_ptr[m_size] = '\0';
		return (const char*)m_ptr;
	}

	PUCHAR begin() const noexcept
	{
		return m_ptr;
	}

	PUCHAR end() const noexcept
	{
		return m_ptr + m_cap;
	}

	PIX& erase()
	{
		m_size = 0;
		return *this;
	}

	PIX slice(const size_t offIdx, const size_t psize)
	{
		assert(offIdx <= m_size && psize <= m_size && offIdx + psize <= m_size);
		PIX temp{data(offIdx), psize};
		return std::move(temp);
	}
	
	bool isMyPointer(const void* p, const size_t size) const
	{
		if (p < begin() || p > end())
		{
			assert(p > end() || (PUCHAR)p + size < begin());
			return false;
		}
		assert((PUCHAR)p + size <= end());
		return true;
	}
	
	PIX& cleanup() noexcept
	{
		delete m_ptr;
		m_ptr  = nullptr;
		m_size = 0;
		m_cap  = 0;

		return *this;
	}

	bool grow(const size_t grow_size)
	{
		size_t left_size = m_cap - m_size;
		if (left_size >= grow_size)
		{
			return true;
		}
		size_t new_cap = m_cap + (grow_size - left_size) + m_size;
		PUCHAR new_ptr = new UCHAR[new_cap + 1];
		if (new_ptr == nullptr)
		{
			_X_PANIC("Out of memory!\r\n");
			return false;
		}
		memcpy(new_ptr, m_ptr, m_size);
		delete m_ptr;
		m_ptr = new_ptr;
		m_cap = new_cap;

		return true;
	}

	PIX& combine(const void* pdata, const size_t size, const size_t delength, const size_t offIdx)
	{
		assert(pdata == nullptr || !isMyPointer(pdata, size));
		if (size > delength)
		{
			// add more than delete.
			size_t add_size = size - delength;
			bool b = grow(add_size);
			if (!b)
			{
				throw std::bad_alloc();
			}
			memcpy(data(offIdx), pdata, delength);
			memmove(data() + (offIdx+size), data(offIdx+delength), m_size-offIdx-delength);
			memcpy(data(offIdx+delength), (PUCHAR)pdata + delength, add_size);
			m_size += add_size;
		}
		else
		{
			memcpy(data(offIdx), pdata, size);
			memmove(data() + (offIdx+size), data(offIdx+delength), m_size-offIdx-delength);
			m_size -= (delength - size);
		}
		return *this;
	}

	PIX& pop(const size_t pop_size)
	{
		assert(pop_size <= m_size);
		return combine(nullptr, 0, pop_size, 0);
	}
	
	PIX& push(const void* pdata, const size_t push_size)
	{
		return combine(pdata, push_size, 0, 0);
	}

	PIX& append(const void* pdata, const size_t psize)
	{
		return combine(pdata, psize, 0, m_size);
	}

	PIX& remove(const size_t offIdx, const size_t rsize)
	{
		return combine(nullptr, 0, rsize, offIdx);
	}

	size_t find(const void* pdata, const size_t psize)
	{
		if (psize > m_size)
		{
			return npos;
		}
		for (size_t i = 0; i <= (m_size - psize); i++)
		{
			if (memcmp(pdata, m_ptr + i, psize) == 0)
			{
				return i;
			}
		}
		return npos;
	}

	size_t find(const void* pdata, const size_t psize, const size_t offIdx)
	{
		if (offIdx >= m_size || psize > (m_size - offIdx))
		{
			return npos;
		}
		for (size_t i = offIdx; i < (m_size - psize); i++)
		{
			if (memcmp(pdata, m_ptr + i, psize) == 0)
			{
				return i;
			}
		}
		return npos;
	}

	size_t rfind(const void* pdata, const size_t psize)
	{
		if (psize > m_size)
		{
			return npos;
		}
		for (size_t i = (m_size - psize); i >= 0; i--)
		{
			if (memcmp(pdata, m_ptr + i, psize) == 0)
			{
				return i;
			}
		}
		return npos;
	}

	PIX& replace(const void* pa, const size_t size_a, const void* pb, const size_t size_b)
	{
		size_t pos = find(pa, size_a);
		if (pos == npos)
		{
			return *this;
		}
		return combine(pb, size_b, size_a, pos);
	}

	PIX& replaceAll(const void* pa, const size_t size_a, const void* pb, const size_t size_b)
	{
		size_t pos = find(pa, size_a);
		while (pos != npos)
		{			 
			combine(pb, size_b, size_a, pos);
			pos += size_b;
			pos = find(pa, size_a, pos);
		}

		return *this;
	}
};