#include "rdix.hxx"
#include "dix_def.hxx"
#include <locale>
#include "dix_utils.hxx"
#include <thread>
#include <future>

using namespace std;

constexpr USHORT SERVER_PORT = 80;
#define ROOT_DIR "D:\\rdix"

static string map_to_local(const string& url)
{
	int pos = url.find("/");
	if (pos == string::npos)
	{
		return ROOT_DIR;
	}
	else
	{
		pos++;
		string dir = url.substr(pos);
		char* tdir = (char*)dir.c_str();
		for (size_t i = 0; i < dir.size(); i++)
		{
			if (tdir[i] == '/')
			{
				tdir[i] = '\\';
			}
		}
		dir = string(ROOT_DIR) + "\\" + dir;
		return dir;
	}
}

bool WEB_SESSION::send_word(string sword)
{
	string to_send = dix_req_handler(sword.c_str());
	FILE* file = fopen("D:\\rdix\\HTML\\rdix.html", "rb");
	_X_ASSERT(file);
	char* buffer = new char[8192];

	HTTP_RESPONSE hresp;
	hresp.set_code(200, "OK");
	hresp.add("Content-Type", "text/html");
	hresp.add("Connection", "close");

	fseek(file, 0, SEEK_END);
	int fileLen = ftell(file);
	fseek(file, 0, SEEK_SET);
	fread(buffer, 1, fileLen, file);
	buffer[fileLen] = '\0';

	string t = buffer;
	string tag = "<!--Below is the documentation part.-->";
	size_t pos = t.find(tag);
	_X_ASSERT(pos != string::npos);
	pos = pos + tag.size() + 1;
	t.replace(pos, 1, to_send);
	for (size_t i = pos; i < t.size(); i++)
	{
		if (t[i] == '\r')
		{
			t.replace(i, 2, "<br>", 4);
		}
		else if (t[i] == '\n')
		{
			t.replace(i, 1, "<br>", 4);
		}
	}
	t = hresp.render() + t;
	send(m_fd, t.c_str(), t.size(), 0);

	return true;
}


void WEB_SESSION::worker()
{
	PIX pix;
	char* buffer = new char[8192];
	while (true)
	{
		int i = recv(m_fd, buffer, 8191, 0);
		if (i <= 0)
		{
			if (i < 0)
			{
				_X_PANIC("recv failed -- %d\r\n", WSAGetLastError());
			}
			break;
		}
		pix.append(buffer, i);

		// check if all headers have been received.
		auto pos = pix.find("\r\n\r\n", 4);
		if (pos != PIX::npos)
		{
			m_req.parser(pix.slice(0, pos + 4).c_str());
			pix.pop(pos + 4);
			auto h = m_req.lookup("Content-Length");
			if (h.size())
			{
				size_t ulen = strtoul(h.c_str(), NULL, 10);
				while (ulen > pix.size())
				{ 
					i = recv(m_fd, buffer, 8191, 0);
					if (i <= 0)
					{
						if (i < 0)
						{
							_X_PANIC("recv failed -- %d\r\n", WSAGetLastError());
						}
						break;
					}
					pix.append(buffer, i);
				}
				if (ulen > pix.size())
				{
					_X_PANIC("Incomplete content.\r\n");
					break;
				}
			}
			if (!req_handler(pix))
			{
				break;
			}
		}
		else
		{
			// need more.
			_X_TRACE("Incomplete HTTP header.\r\n");
			printf("%s\r\n", pix.c_str());
		}
	}
}

bool WEB_SESSION::req_handler(PIX& pbody)
{
	if (m_req.check_verb("GET"))
	{
		return doGET(pbody);
	}
	else if (m_req.check_verb("POST"))
	{
		return doPOST(pbody);
	}
	msg_sender(404, "verb not valid.");
	return false;
}

bool WEB_SESSION::doGET(PIX& pbody)
{
	string url   = m_req.get_url();
	// printf("URL:%s\r\n", url.c_str());
	string local = map_to_local(url);
	int pos = local.find("?");
	if (pos == string::npos)
	{
		FILE* file = fopen(local.c_str(), "rb");
		if (!file)
		{
			msg_sender(404, "file not found.");
			return false;
		}
		HTTP_RESPONSE hresp;
		hresp.set_code(200, "OK");
		hresp.add("Content-Length", std::to_string(get_filesize(file)));
		hresp.add("Connection", "close");
		hresp.add("Content-Type", "text/html");

		string resp_msg = hresp.render();
		send(m_fd, resp_msg.c_str(), resp_msg.size(), 0);
		while (!feof(file))
		{
			char buffer[2048];
			int i = fread(buffer, 1, 2048, file);
			if (i <= 0)
			{
				_X_PANIC("fread failed.\r\n");
				break;
			}
			i = send(m_fd, buffer, i, 0);
			if (i <= 0)
			{
				_X_PANIC("send failed.\r\n");
				break;
			}
		}
		fclose(file);
	}
	else
	{
		map<string, string> args;
		m_req.get_extra_info(args);
		if (!args.empty())
		{	
			/*
			for (const auto& p : args)
			{
				printf("name:[%s], val:[%s]\r\n", p.first.c_str(), p.second.c_str());
			}
			*/
			if (args.find("word") == args.end())
			{
				_X_PANIC("Did not send the word.\r\n");
			}
			else
			{
				send_word(args["word"]);
			}
		}
	}
	return false;
}

bool WEB_SESSION::doPOST(PIX& pbody)
{
	return false;
}

static void server_thread(SOCKET fd)
{
	WEB_SESSION web{fd};
	web.worker();
	return ;

	/*
	char buffer[2048];
	int i = recv(fd, buffer, 2047, 0);
	if (i <= 0)
	{
		if (i != 0)
		{
			_X_PANIC("recv failed -- %d\r\n", WSAGetLastError());
		}
		closesocket(fd);
		return ;
	}
	buffer[i] = '\0';		// terminate with null char.
	HTTP_REQUEST hreq;
	hreq.parser(buffer);
	string dir = map_to_local(hreq.get_url());
	printf("----------request--------\r\n");
	hreq.dump();
	printf("-------------------------\r\n");
	{
		HTTP_RESPONSE hresp;
		
		FILE* file = fopen(dir.c_str(), "rb");
		if (!file)
		{
			// local file does not exist.
			_X_TRACE("has no file for [%s]\r\n", hreq.get_url().c_str());
			hresp.set_code(404, "Not Found");
			hresp.set_version(1, 1);
			hresp.add("Connection", "close").add("Content-Type", "text/html");
			buffer[0] = '\0';
		}
		else
		{
			hresp.set_code(200, "OK");
			hresp.set_version(1, 1);
			hresp.add("Connection", "close").add("Content-Type", "text/html");

			fseek(file, 0, SEEK_END);
			int fileLen = ftell(file);
			fseek(file, 0, SEEK_SET);
			fread(buffer, 1, fileLen, file);
			buffer[fileLen] = '\0';

			string t = buffer;
			string tag = "<!--Below is the documentation part.-->";
			size_t pos = t.find(tag);
			if (pos != string::npos)
			{
				pos = pos + tag.size() + 1;
				t.replace(pos, 1, dix_req_handler("predominate"));
				for (size_t i = pos; i < t.size(); i++)
				{
					if (t[i] == '\r')
					{
						t.replace(i, 2, "<br>", 4);
					}
					else if (t[i] == '\n')
					{
						t.replace(i, 1, "<br>", 4);
					}
				}
				strcpy(buffer, t.c_str());
			}
		}

		string s = hresp.render() + buffer;
		// printf("%s\r\n", s.c_str());
		i = ::send(fd, s.c_str(), (int)s.size(), 0);
	}
	closesocket(fd);
	*/

	/*
	string s = dix_req_handler(buffer);
	//closesocket(fd);
	//printf("%s\r\n", s.c_str());
	//return s.c_str();
	i = ::send(fd, s.c_str(), s.size(), 0);
	if (i <= 0)
	{
		_X_PANIC("send failed -- %d\r\n", WSAGetLastError());
	}
	closesocket(fd);
	*/
}

static void dix_server()
{
	SOCKET lfd = socket(PF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port	= htons(SERVER_PORT);
	addr.sin_addr.S_un.S_addr = 0;
	int i = ::bind(lfd, (const sockaddr*)&addr, sizeof(addr));
	if (i != NOERROR)
	{
		_X_PANIC("bind failed -- %d\r\n", WSAGetLastError());
		closesocket(lfd);
		exit(-1);
	}

	listen(lfd, 64);
	do
	{
		i = sizeof(addr);
		SOCKET fd = accept(lfd, (sockaddr*)&addr, &i);
		if (fd == INVALID_SOCKET)
		{
			_X_PANIC("accept failed -- %d\r\n", WSAGetLastError());
			continue;
		}
		_X_TRACE("new connection from: %s:%d\r\n",inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
		
		thread th{server_thread, fd};
		th.detach();				
	}while(true);

	closesocket(lfd);
}

static void client_handler(const char* s)
{
	SOCKET fd = socket(PF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = 0;
	addr.sin_addr.S_un.S_addr = 0;
	int i = ::bind(fd, (const sockaddr*)&addr, sizeof(addr));
	_X_ASSERT(i == NOERROR);

	addr.sin_port = htons(SERVER_PORT);
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	i = connect(fd, (const sockaddr*)&addr, sizeof(addr));
	if (i != NOERROR)
	{
		_X_PANIC("failed connect to dix server -- %d\r\n", WSAGetLastError());
		closesocket(fd);
		return ;
	}

	i = send(fd, s, (int)strlen(s), 0);
	if (i <= 0)
	{
		_X_PANIC("send failed -- %d\r\n", WSAGetLastError());
		closesocket(fd);
		return ;
	}

	char* buffer = new char[8192];
	while (true)
	{
		i = recv(fd, buffer, 8191, 0);
		if (i <= 0)
		{
			if (i < 0)
			{
				_X_PANIC("recv failed -- %d\r\n", WSAGetLastError());				
			}
			break;
		}
		buffer[i] = '\0';
		printf("%s", buffer);
	}

	delete[] buffer;
	closesocket(fd);
}

static void do_client_repl()
{
	char buffer[2048];
	while(true)
	{
		printf("please enter: ");
		gets_s(buffer);
		if (strcmp(buffer, "exit") == 0)
		{
			break;
		}
		client_handler(buffer);
	}
}

void tester()
{
	string s = "ruix";
	PIX pix{"abcdefg"};
	pix << "hijklmn" << s;
	PIX pix2;
	pix2 = pix + " 123545";

	printf("%s\r\n", pix.c_str());
	printf("%s\r\n", pix2.c_str());

	pix2 += pix;
	printf("%s\r\n", pix2.c_str());
}

int main()
{
	WSADATA w;
	WSAStartup(0x0202, &w);

	setlocale(LC_CTYPE, ".UTF-8");
	/*
	thread t{ init_dixmap };
	t.detach();
	*/
	init_dixmap();
	

	//dix_repl();	
	thread th{dix_server};
	th.detach();	

	//dix_repl();
	do_client_repl();
	

	WSACleanup();

	return 0;
}
