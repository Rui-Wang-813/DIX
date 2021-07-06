#pragma once
#include <WinSock2.h>
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <map>
#include <string>
using namespace std;
#include "dix_utils.hxx"

class HTTP_MSG
{
protected:
	map<string, string> m_table;
public:
	HTTP_MSG() noexcept {};
	HTTP_MSG(const HTTP_MSG& _other) noexcept
		:m_table{ _other.m_table }
	{}
	HTTP_MSG(HTTP_MSG&& _other) noexcept
		:m_table{ std::move(_other.m_table) }
	{}
	virtual ~HTTP_MSG()
	{}
	HTTP_MSG& operator = (const HTTP_MSG& _right) noexcept
	{
		m_table = _right.m_table;
		return *this;
	}
	HTTP_MSG& operator = (HTTP_MSG&& _right) noexcept
	{
		m_table = std::move(_right.m_table);
		return *this;
	}

	string lookup(const string name)
	{
		auto p = m_table.find(name);
		if (p != m_table.end())
		{
			return p->second;
		}
		return "";
	}

	HTTP_MSG& add(const string name, const string val)
	{
		auto s = lookup(name);
		if (s.empty())
		{
			m_table[name] = val;
		}
		else
		{
			m_table[name] += "; " + val;
		}

		return *this;
	}

	HTTP_MSG& remove(const string name)
	{
		auto s = lookup(name);
		if (!s.empty())
		{
			m_table.erase(name);
		}

		return *this;
	}

	virtual string render()
	{
		string s = "";
		for (auto p : m_table)
		{
			s += p.first;
			s += ": ";
			s += p.second;
			s += "\r\n";
		}
		s += "\r\n";

		return std::move(s);
	}

	/*
	HTTP_MSG& parser(const string s)
	{
		size_t pos = 0;
		while (pos < s.size())
		{
			size_t tpos = s.find("\r\n", pos);
			string line = s.substr(pos, tpos - pos);

			int split_pos = s.find(":", pos);
			string name = s.substr(pos, split_pos - pos);
			split_pos++;
			while (s[split_pos] == ' ')
			{
				++split_pos;
			}
			string val  = s.substr(split_pos, tpos - split_pos);
			
			add(name, val);
			pos = tpos + 2;
		}

		return *this;
	}
	*/

	virtual bool parser(const string& s)
	{
		char* ptr = (char*)s.c_str();
		const char* tail = ptr + s.size();
		const char* prev;

		while (ptr < tail)
		{
			while (ptr < tail && strchr(" \t\r\n", *ptr))
			{
				ptr++;
			}
			if (ptr == tail)
			{
				break;
			}
			_X_ASSERT(ptr < tail);
			prev = ptr;
			while (ptr < tail && *ptr != ':')
			{
				ptr++;
			}
			if (ptr == tail)
			{
				_X_PANIC("Invalid name.\r\n");
				break;
			}
			*ptr = '\0';
			string name = prev;
			*ptr = ':';
			ptr++;
			while (ptr < tail && *ptr == ' ')
			{
				ptr++;
			}
			if (ptr == tail)
			{
				_X_PANIC("Invalid value.\r\n");
				break;
			}
			prev = ptr;
			while (ptr < tail && strchr("\r\n", *ptr) == NULL)
			{
				ptr++;
			}
			if (ptr == tail)
			{
				_X_PANIC("No crlf token.\r\n");
				break;
			}
			char t = *ptr;
			*ptr = '\0';
			string val = prev;
			*ptr = t;
			add(name, val);
		}

		return true;
	}

	void dump()
	{
		string s = render();
		_X_TRACE("%s", s.c_str());
	}

	bool version_crack(const string& s, int& vmajor, int& vminor)
	{
		auto tpos = s.find('/');
		if (tpos == s.npos)
		{
			return false;
		}
		const char* p = s.c_str() + tpos + 1;

		// HTTP/1.1
		char* tp;
		vmajor = strtoul(p, &tp, 10);
		if (tp == NULL || *tp != '.')
		{
			return false;
		}
		vminor = strtoul(tp + 1, NULL, 10);
		if (vminor < 0)
		{
			return false;
		}

		return true;
	}
};

class HTTP_REQUEST : public HTTP_MSG
{
protected:
	string m_verb;
	string m_url;
	int m_vmajor;
	int m_vminor;
public:
	using HTTP_MSG::HTTP_MSG;
	HTTP_REQUEST(const HTTP_REQUEST& _other)
		:HTTP_MSG(_other)
		,m_verb(_other.m_verb)
		,m_url(_other.m_url)
		,m_vmajor(_other.m_vmajor)
		,m_vminor(_other.m_vminor)
	{}

	string get_url()
	{
		return m_url;
	}
	bool check_verb(const char* szverb)
	{
		if (m_verb.compare(szverb) == 0)
		{
			return true;
		}
		return false;
	}

	bool parser(const string& s) override
	{
		size_t tpos = s.find('\n');
		string rline = s.substr(0, tpos);
		if (rline.back() == '\r')
		{
			rline = rline.substr(0, rline.size() - 1);
		}

		if (!HTTP_MSG::parser(s.substr(tpos + 1)))
		{
			return false;
		}

		tpos = rline.find(' ');
		m_verb = rline.substr(0, tpos);

		auto pos = tpos + 1;
		tpos = rline.find(' ', pos);
		m_url = rline.substr(pos, tpos - pos);
		
		// HTTP/1.1
		pos = tpos + 1;
		if (!version_crack(rline.substr(pos), m_vmajor, m_vminor))
		{
			return false;
		}

		return true;
	}

	string render() override
	{
		string s = HTTP_MSG::render();
		string t = m_verb + " " + m_url + " " + "HTTP/";
		t += std::to_string(m_vmajor) + "." + std::to_string(m_vminor);
		
		return t + "\r\n" + s;
	}
	void _parse_pair(map<string, string>& args, const string& token)
	{
		int pos = token.find("=");
		if (pos == string::npos)
		{
			args[token] = "";
			return ;
		}
		string name = token.substr(0, pos);
		string val  = token.substr(pos + 1);
		if (args.find(name) != args.end())
		{
			args[name] += "; " + val;
		}
		else
		{
			args[name] = val;
		}
	}
	void get_extra_info(map<string, string>& args)
	{
		size_t pos = m_url.find("?");
		if (pos == string::npos)
		{
			return ;
		}
		pos++;
		while (pos < m_url.size())
		{
			size_t tpos = m_url.find("&", pos);
			if (tpos == string::npos)
			{
				_parse_pair(args, m_url.substr(pos));
				break;
			}
			_parse_pair(args, m_url.substr(pos, tpos - pos));
			pos = tpos + 1;
		}
	}
};

class HTTP_RESPONSE : public HTTP_MSG
{
protected:
	int m_vmajor{ 1 };
	int m_vminor{ 1 };
	int m_code;
	string m_reason;
public:
	using HTTP_MSG::HTTP_MSG;

	bool parser(const string& s) override
	{
		size_t tpos = s.find('\n');
		string rline = s.substr(0, tpos);
		if (rline.back() == '\r')
		{
			rline = rline.substr(0, rline.size() - 1);
		}

		if (!HTTP_MSG::parser(s.substr(tpos + 1)))
		{
			return false;
		}

		// HTTP/1.1 CODE reason
		tpos = rline.find(' ');
		if (!version_crack(rline.substr(0, tpos), m_vmajor, m_vminor))
		{
			return false;
		}

		auto pos = tpos + 1;
		char* tp;
		m_code = strtoul(rline.c_str() + pos, &tp, 10);
		if (tp == NULL || (*tp != ' ' && *tp != '\0'))
		{
			return false;
		}
		if (*tp != '\0')
		{
			++tp;
		}

		m_reason = tp;

		return true;
	}

	string render() override
	{
		string s = HTTP_MSG::render();
		string t = "HTTP/" + std::to_string(m_vmajor) + "." + std::to_string(m_vminor) + " ";
		t += std::to_string(m_code);
		if (m_reason.size())
		{
			t += " " + m_reason;
		}

		return t + "\r\n" + s;
	}

	void set_code(const int code)
	{
		m_code = code;
		m_reason = "";
	}
	void set_code(const int code, const string reason)
	{
		m_code = code;
		m_reason = reason;
	}
	void set_version(const int vmajor, const int vminor)
	{
		m_vmajor = vmajor;
		m_vminor = vminor;
	}
};

class WEB_SESSION
{
protected:
	SOCKET m_fd;
	HTTP_REQUEST m_req;
public:
	WEB_SESSION(SOCKET fd)
		:m_fd(fd)
	{}
	~WEB_SESSION()
	{
		closesocket(m_fd);
	}

	void worker();

protected:
	bool req_handler(PIX& pbody);
	bool doGET(PIX& pbody);
	bool doPOST(PIX& pbody);
	void msg_sender(UINT code, string reason)
	{
		HTTP_RESPONSE hresp;
		hresp.set_code(code, reason);
		hresp.set_version(1, 1);
		hresp.add("Connection", "close");
		hresp.add("Content-Type", "text/html");
		
		string msg = hresp.render();
		send(m_fd, msg.c_str(), (int)msg.size(), 0);
	}
	bool send_word(string sword);
};