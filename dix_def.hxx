#pragma once
#include "rdix.hxx"
#include <string>
#include <vector>
#include <algorithm>
#include <Windows.h>
using namespace std;

struct IPA
{
	bool		bUSA;
	std::string	ipa;
	std::string mp3_url;
};

struct SIMPLE
{
	std::string attr;	// optional.
	std::string text;	
};

struct C_DEF
{
	std::string attr;
	std::string cn_desc;
	std::string en_def;
};

struct C_SAMPLE
{ 
	std::string en_text;
	std::string cn_text;
};

struct DIX_WORD
{
	std::string				name;
	std::vector<IPA>		ipas;
	std::vector<SIMPLE>		simples;
	std::vector<C_DEF>		defs;
	std::vector<C_SAMPLE>	samples;	
};

struct TRIE_NODE
{
	char ch;
	std::vector<TRIE_NODE> children;
	bool isWord;

	TRIE_NODE(const char _ch, const bool _isWord)
		:ch{_ch}
		,isWord{_isWord}
	{}

	void addWord(const char* name)
	{
		if (name == NULL || *name == 0)
		{
			return;
		}
		char c  = name[0];
		auto it = std::find_if(children.begin(), children.end(), [c](TRIE_NODE& child)
					 {
						 return child.ch == c;
					 });
		if (it == children.end())
		{
			TRIE_NODE new_node{c, name[1] == '\0'};
			new_node.addWord(&name[1]);
			children.push_back(new_node);			
		}		
		else
		{
			if (name[1] == '\0')
			{
				it->isWord = true;
				return ;
			}
			it->addWord(&name[1]);
		}
	}

	void addAllWords(vector<string>& v, string prefix)
	{
		if (isWord)
		{
			v.push_back(prefix);
		}
		for (auto& p : children)
		{
			p.addAllWords(v, prefix + p.ch);
		}		
	}

	void searchPrefix(const char* prefix, vector<string>& v)
	{
		if (prefix == NULL || *prefix == '\0')
		{
			addAllWords(v, "");
		}
		else
		{
			const char c = prefix[0];
			auto it = std::find_if(children.begin(), children.end(), [c](TRIE_NODE& child)
								   {
									   return child.ch == c;
								   });
			if (it == children.end())
			{
				return ;
			}
			else
			{
				it->searchPrefix(&prefix[1], v);
				for (string& s : v)
				{
					s = prefix[0] + s;
				}
			}
		}
	}	
};

struct COR_HELPER		// helper to correct words only.
{
	string s;
	int dist;
};

void init_dixmap();
const DIX_WORD* dix_lookup(std::string name);
string dix_req_handler(const char* sreq);