#include "dix_def.hxx"
#include <unordered_map>
#include "dix_utils.hxx"
#include <Windows.h>
#include <algorithm>
#include <mutex>


using DIX_MAP  = std::unordered_map<std::string, DIX_WORD>;
using DIX_LIST = std::vector<std::string>;

#define NUM_THREADS 8

DIX_MAP g_dixmap;
DIX_LIST g_dixlist;
TRIE_NODE g_root{'\0', false};

mutex g_dix_syn;

std::vector<std::string> splitter(const std::string& s)
{ 
	std::string temp = trim(s, " \t\r\n<>");

	std::vector<std::string> v;
	size_t pos = 0;
	do
	{
		size_t tpos = temp.find("><", pos);
		if (tpos == temp.npos)
		{
			v.push_back(std::move(temp.substr(pos)));
			break;
		}
		v.push_back(std::move(temp.substr(pos, tpos - pos)));
		pos = tpos + 2;
	}while(true);

	return std::move(v);
}

void parser(std::vector<std::string>& u)
{
	std::string& name = u[0];
	std::string& tag  = u[1];

	if (tag == "$IPA")
	{
		// <name><$IPA><英/美 [name ipa]><MP3 url>
		_X_ASSERT(u.size() == 4);

		IPA ipa;

		// Get the bUSA set.
		_X_ASSERT(u[2].length() > 1);
		if (u[2].substr(0, 3) == u8"英")
		{
			ipa.bUSA = false;
		}
		else if (u[2].substr(0, 3) == u8"美")
		{
			ipa.bUSA = true;
		}
		else
		{
			//_X_PANIC("Not valid form! [%s]", u[2].c_str());
			ipa.bUSA = false;
		}

		ipa.ipa = u[2].substr(1);
		ipa.mp3_url = u[3];

		// Push into the ipas vector.
		g_dixmap[name].ipas.push_back(std::move(ipa));
	}
	else if (tag == "$SIMPLE")
	{
		// <name><$SIMPLE><attr. text>
		_X_ASSERT(u.size() == 3);

		SIMPLE simple;

		size_t pos = 0, tpos = u[2].find(".");
		if (tpos == std::string::npos)
		{
			tpos = 0;
		}

		simple.attr = u[2].substr(pos, tpos - pos);
		simple.text = u[2].substr(tpos);

		g_dixmap[name].simples.push_back(std::move(simple));
	}
	else if (tag == "$C_DEF")
	{
		// <name><$C_DEF><attr><cn_desc><en_def>
		_X_ASSERT(u.size() == 5);

		C_DEF cdef;

		cdef.attr = u[2];
		cdef.cn_desc = u[3];
		cdef.en_def = u[4];

		g_dixmap[name].defs.push_back(std::move(cdef));
	}
	else if (tag == "$C_SAMPLE")
	{
		// <name><$C_SAMPLE><en_text><cn_text>
		_X_ASSERT(u.size() == 4);

		C_SAMPLE sample;

		sample.en_text = u[2];
		sample.cn_text = u[3];

		g_dixmap[name].samples.push_back(std::move(sample));
	}
	else
	{
		_X_PANIC("What is this? word[%s] tag[%s]\r\n", name.c_str(), tag.c_str());
	}
}

void build_trie()
{
	for (auto it = g_dixmap.begin(); it != g_dixmap.end(); it++)
	{
		g_root.addWord(it->first.c_str());
	}
}

void init_dixmap()
{
	FILE* file = fopen("tdefs.txt", "r");
	if (!file)
	{
		_X_PANIC("Failed to open file:%s\r\n", "tdefs.txt");
		_X_ASSERT(file);
		return ;
	}

	printf("Loading Dix map...\r\n");
	DWORD init_tick = GetTickCount();

	std::vector<char> v;
	v.reserve(8192);
	while (!feof(file))
	{		
		fgets(v.data(), 8192, file);

		std::string s(v.data());

		std::vector<std::string> u = splitter(s);
		_X_ASSERT(u.size() >= 2);				

		std::string& name = u[0];
		if (g_dixmap.find(name) == g_dixmap.end())
		{
			DIX_WORD new_word;
			new_word.name = name;
			g_dixmap[name] = new_word;
			//printf("word [%s] added.\r\n", name.c_str());
			g_dixlist.push_back(name);
		}
		parser(u);
	}
	fclose(file);

	build_trie();

	init_tick = GetTickCount() - init_tick;
	printf("Take tick %d seconds %d.\r\n", init_tick, init_tick / 1000);
}

const DIX_WORD* dix_lookup(std::string name)
{
	auto p = g_dixmap.find(name);
	if (p == g_dixmap.end())
	{
		return nullptr;
	}
	return &(p->second);
}

/*
void dump(const DIX_WORD* dix_word)
{
	printf("NAME: [%s]\r\n", dix_word->name.c_str());

	// dump IPAs.
	for (const auto& p : dix_word->ipas)
	{
		color_printf(FG_YELLOW, "MP3: [%s] IPA: %s%s\r\n", p.mp3_url.c_str(), p.bUSA ? u8"美" : u8"英", p.ipa.c_str());
	}
	printf("\r\n");

	// dump SIMPLEs.
	for (const auto& p : dix_word->simples)
	{
		color_printf(FG_GREEN, "SIMPLE: [%s.] [%s]\r\n", p.attr.c_str(), p.text.c_str());
	}
	printf("\r\n");

	// dump DEFs.
	for (const auto& p : dix_word->defs)
	{
		color_printf(FG_BLUE, "DEF: [%s] [%s] [%s]\r\n", p.attr.c_str(), p.cn_desc.c_str(), p.en_def.c_str());
	}
	printf("\r\n");

	// dump SAMPLEs.
	for (const auto& p : dix_word->samples)
	{
		color_printf(FG_PURPLE, "SAMPLE: [%s] [%s]\r\n", p.en_text.c_str(), p.cn_text.c_str());
	}
	printf("\r\n");
}
*/

string dump(const DIX_WORD* dix_word)
{
	char* buffer = new char[8192];
	string holder;
	holder.reserve(16384);

	snprintf(buffer, 8192, "NAME: [%s]\r\n", dix_word->name.c_str());
	holder += buffer;
	// dump IPAs.
	for (const auto& p : dix_word->ipas)
	{
		snprintf(buffer, 8192, "MP3: [%s] IPA: %s%s\r\n", p.mp3_url.c_str(), p.bUSA ? u8"美" : u8"英", p.ipa.c_str());
		holder += buffer;
	}
	snprintf(buffer, 8192, "\r\n");
	holder += buffer;

	// dump SIMPLEs.
	for (const auto& p : dix_word->simples)
	{
		snprintf(buffer, 8192, "SIMPLE: [%s.] [%s]\r\n", p.attr.c_str(), p.text.c_str());
		holder += buffer;
	}
	snprintf(buffer, 8192, "\r\n");
	holder += buffer;

	// dump DEFs.
	for (const auto& p : dix_word->defs)
	{
		snprintf(buffer, 8192, "DEF: [%s] [%s] [%s]\r\n", p.attr.c_str(), p.cn_desc.c_str(), p.en_def.c_str());
		holder += buffer;
	}
	snprintf(buffer, 8192, "\r\n");
	holder += buffer;

	// dump SAMPLEs.
	for (const auto& p : dix_word->samples)
	{
		snprintf(buffer, 8192, "SAMPLE: [%s] [%s]\r\n", p.en_text.c_str(), p.cn_text.c_str());
		holder += buffer;
	}
	snprintf(buffer, 8192, "\r\n");
	holder += buffer;

	return holder;
}

/*
void prefixHandler(string prefix)
{ 
	vector<string> v;
	g_root.searchPrefix((const char*)prefix.c_str(), v);
	for (auto& p : v)
	{
		printf("%s\r\n", p.c_str());
	}
	printf("count: %d\r\n", v.size());
}
*/

string prefixHandler(string prefix)
{
	vector<string> v;
	g_root.searchPrefix((const char*)prefix.c_str(), v);

	string s = "";
	for (auto& p : v)
	{
		// printf("%s\r\n", p.c_str());
		s += p + "\r\n";
	}
	// printf("count: %d\r\n", v.size());
	s += "count: " + std::to_string(v.size()) + "\r\n";

	return s;
}

string searchHandler(string name)
{
	const auto pdix = dix_lookup(name);
	if (!pdix)
	{
		return "";
	}
	string s = dump(pdix);
	return s;
}

int calcEdit(string s1, string s2)
{
	if (s1.length() == 0 && s2.length() == 0)
	{
		return 0;
	}
	if (s1.length() == 0)
	{
		return s2.length();
	}
	if (s2.length() == 0)
	{
		return s1.length();
	}

	int len1 = s1.length(), len2 = s2.length();
	char c1 = s1[len1 - 1], c2 = s2[len2 - 1];

	if (c1 == c2)
	{
		return calcEdit(s1.substr(0, len1 - 1), s2.substr(0, len2 - 1));
	}
	else
	{
		int a = calcEdit(s1.substr(0, len1 - 1), s2.substr(0, len2 - 1));
		int b = calcEdit(s1.substr(0, len1 - 1), s2);
		int c = calcEdit(s1, s2.substr(0, len2 - 1));
		return min(a, min(b, c)) + 1;
	}
}

/*
void editThread(COR_HELPER* candidates, string name, size_t pos)
{
	for (int i = pos; i < pos + (g_dixlist.size() / NUM_THREADS); i++)
	{
		string s = g_dixlist[i];
		int dist = calcEdit(s, name);
		g_dix_syn.lock();
		if (dist > candidates[9].dist)
		{
			continue;
		}
		g_dix_syn.unlock();
		for (int i = 0; i < 10; i++)
		{
			g_dix_syn.lock();
			auto& c = candidates[i];
			if (dist < c.dist)
			{
				for (int j = 9; j > i; j--)
				{
					candidates[j].s = candidates[j - 1].s;
					candidates[j].dist = candidates[j - 1].dist;
				}
				candidates[i].s    = s;
				candidates[i].dist = dist;
				break;
			}
			g_dix_syn.unlock();
		}
	}
}
*/

void editHandler(string name, COR_HELPER* candidates)
{
	for (int i = 0; i < 10; i++)
	{
		auto& c = candidates[i];
		c.s = "";
		c.dist = INT_MAX;
	}
	for (auto p : g_dixmap)
	{
		int dist = calcEdit(p.first, name);
		if (dist > candidates[9].dist)
		{
			continue;
		}
		for (int i = 0; i < 10; i++)
		{
			auto& c = candidates[i];
			if (dist < c.dist)
			{
				for (int j = 9; j > i; j--)
				{
					candidates[j].s = candidates[j - 1].s;
					candidates[j].dist = candidates[j - 1].dist;
				}
				candidates[i].s = p.first;
				candidates[i].dist = dist;
				break;
			}
		}
	}
	/*
	for (int i = 0; i < NUM_THREADS; i++)
	{
		thread th{editThread, candidates, name, g_dixlist.size() * i / NUM_THREADS};
		th.detach();
	}
	*/

	printf("Are you trying to search: \r\n");
	for (int i = 0; i < 10; i++)
	{
		auto c = candidates[i];
		if (c.dist == INT_MAX)
		{
			break;
		}
		printf("word: %s, edit distance: %d\r\n", c.s.c_str(), c.dist);
	}
}

/*
void dix_repl()
{
	char temp[256];
	while (true)
	{
		printf("Please enter: ");
		gets_s(temp);
		if (temp[0] == '\'')
		{
			prefixHandler(&temp[1]);
		}
		else if (!searchHandler(temp))
		{
			printf("No word [%s], searching for similar words...\r\n", temp);

			COR_HELPER candidates[10];
			editHandler(temp, candidates);
			int i = -1;
			while (i < 0 || i > 9)
			{
				printf("Please enter the number: ");
				i = getchar() - '0';
			}
			searchHandler(candidates[i].s);
		}
	}
	return;
}
*/

string dix_req_handler(const char* sreq)
{
	// if (strlen(sreq) > 3 && sreq[0] == '%' && sreq[1] == '2' && sreq[2] == '7')
	if (sreq[0] == '1')
	{
		string s = prefixHandler(&sreq[1]);
		return s;
	}
	else
	{
		string s = searchHandler(sreq);
		if (s.size())
		{
			return s;
		}
		printf("No word [%s], searching for similar words...\r\n", sreq);

		COR_HELPER candidates[10];
		editHandler(sreq, candidates);
		int i = -1;
		while (i < 0 || i > 9)
		{
			printf("Please enter the number: ");
			i = getchar() - '0';
		}
		searchHandler(candidates[i].s);
		return " ";
	}
}