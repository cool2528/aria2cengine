#include "WebHttpRequest.h"
#include "base64.h"

WebHttpRequest::WebHttpRequest()
{
}


WebHttpRequest::~WebHttpRequest()
{
}

std::string WebHttpRequest::GetLogid(bool isFlag)
{
	std::string strResult;
	srand(ULLONG_MAX);
	CHAR szLogid[0x20];
	CHAR szTime[0x20];
	CHAR szResult[0x50];
	ZeroMemory(szLogid, 0x20);
	ZeroMemory(szTime, 0X20);
	ZeroMemory(szResult, 0X50);
	sprintf_s(szLogid, "%I64u", labs(rand()));
	sprintf_s(szTime, "%I64d", getTimeStamp());
	strcat_s(szResult, szTime);
	strcat_s(szResult, "0.");
	if (isFlag)
	{
		if (lstrlenA(szLogid) >= 16)
			szLogid[16] = 0;
	}
	else
	{
		if (lstrlenA(szLogid) >= 16)
			szLogid[17] = 0;
	}
	strcat_s(szResult, szLogid);
	strResult = szResult;
	strResult = aip::base64_encode(strResult.c_str(), strResult.length());
	return strResult;
}

std::string WebHttpRequest::random()
{
	std::string strResult = "0.";
	srand(ULLONG_MAX);
	CHAR szBuf[0x20];
	ZeroMemory(szBuf, 0x20);
	sprintf_s(szBuf, "%I64u", rand());
	if (lstrlenA(szBuf) >= 16)
		szBuf[16] = 0;
	strResult += szBuf;
	return strResult;
}

long long WebHttpRequest::getTimeStamp()
{
	timeb t;
	ftime(&t);
	return t.time * 1000 + t.millitm;
}

std::string WebHttpRequest::GetTextMid(const std::string strSource, const std::string strLeft, const std::string strRight)
{
	std::string strResult;
	int nBeigin = 0, nEnd = 0, nLeft = 0;
	if (strSource.empty())return strResult;
	nBeigin = strSource.find(strLeft);
	if (nBeigin == std::string::npos)return strResult;
	nLeft = nBeigin + strLeft.length();
	nEnd = strSource.find(strRight, nLeft);
	if (nEnd==std::string::npos)
	{
		strResult = strSource.substr(nLeft, strSource.length());
		return strResult;
	}
	strResult = strSource.substr(nLeft, nEnd - nLeft);
	return strResult;
}

std::string WebHttpRequest::GetCookies(const std::string strHeader)
{
	std::string strResult,strTwoResult;
	std::vector<std::string> headerList;
	std::set<std::string> resultList;
	if (!strHeader.empty())
	{
		std::string split = strHeader + "\r\n";
		std::string strsub;
		size_t pos = 0;
		size_t npos = 0;
		npos = split.find("\r\n", pos);
		while (npos != std::string::npos)
		{
			strsub = split.substr(pos, npos - pos);
			pos = npos + strlen("\r\n");
			npos = split.find("\r\n", pos);
			headerList.push_back(strsub);
		}
	}
	for (auto & v:headerList)
	{
		std::string strTmp;
		int nPos = v.find("Set-Cookie:");
		if (nPos!=std::string::npos)
		{
			std::string strTmp = v.substr(nPos + lstrlenA("Set-Cookie:"), v.length() - (nPos + lstrlenA("Set-Cookie:")));;
			if (strTmp.at(strTmp.length()-1) != ';')
			{
				strTmp += ";";
			}
			strResult += strTmp;
		}
	}
	headerList.clear();
	if (!strResult.empty())
	{
		if (strResult.at(strResult.length() - 1) != ';')
			strResult += ";";
		std::string split = strResult;
		std::string strsub;
		size_t pos = 0;
		size_t npos = 0;
		npos = split.find(";", pos);
		while (npos != std::string::npos)
		{
			strsub = split.substr(pos, npos - pos);
			pos = npos + strlen(";");
			npos = split.find(";", pos);
			resultList.insert(strsub);
		}
	}
	else
	{
		return strResult;
	}

	for (auto& s : resultList)
		strTwoResult += s + ";";
	return strTwoResult;
}

std::string WebHttpRequest::DownloadFile(const std::string strUrl, const std::string strSavePath)
{
	std::string strFileName;
	if (strUrl.empty() || strSavePath.empty()) return strFileName;
	CURL *curl_handle;
	FILE *pagefile;
	strFileName = strSavePath;
// 	int nPos = strUrl.rfind("/");
// 	if (nPos != std::string::npos)
// 	{
// 		strFileName += strUrl.substr(nPos + 1, strUrl.length() - nPos);
// 	}
	curl_global_init(CURL_GLOBAL_ALL);
	curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_URL, strUrl.c_str());
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, true);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, false);	// 验证对方的SSL证书
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, false);	//根据主机验证证书的名称
	//curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, progress_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
	pagefile = fopen(strFileName.c_str(), "wb");
	if (pagefile) {
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
		curl_easy_perform(curl_handle);
		fclose(pagefile);
	}
	curl_easy_cleanup(curl_handle);
	curl_global_cleanup();
	return strFileName;
}

std::string WebHttpRequest::FormatText(const char* format, ...)
{
	std::string strRetbuf;
	char szBuffer[1024];
	ZeroMemory(szBuffer, 1024);
	va_list arglist;
	va_start(arglist, format);
	vsprintf_s(szBuffer, format, arglist);
	va_end(arglist);
	strRetbuf = szBuffer;
	return strRetbuf;
}

std::string WebHttpRequest::URL_Coding(const char* szSource, bool isletter, bool isUtf8)
{
	CHAR szTemp[20];
	CHAR* buffer = nullptr;
	std::string strBuffer;
	std::string result;
	std::string strTemp;
	if (isUtf8)
		strBuffer = Gbk_To_Utf8(szSource);
	else
		strBuffer = szSource;
	int lens = strBuffer.length();
	buffer = new CHAR[lens + 1];
	if (!buffer)return result;
	ZeroMemory(buffer, lens + 1);
	memcpy(buffer, strBuffer.c_str(), lens);
	for (int i = 0; i<lens; i++)
	{
		ZeroMemory(szTemp, 20);
		if (isletter)
		{
			BYTE cbyte = *(buffer + i);
			if (cbyte > 44 && cbyte < 58 && 47 != cbyte)		//0-9
			{
				sprintf_s(szTemp, "%c", cbyte);
				strTemp = szTemp;
				result += strTemp;
			}
			else if (cbyte > 64 && cbyte < 91)	//A-Z
			{
				sprintf_s(szTemp, "%c", cbyte);
				strTemp = szTemp;
				result += strTemp;
			}
			else if (cbyte > 96 && cbyte < 123)	//a-z
			{
				sprintf_s(szTemp, "%c", cbyte);
				strTemp = szTemp;
				result += strTemp;
			}
			else
			{
				sprintf_s(szTemp, "%02X", *(PBYTE)(buffer + i));
				strTemp = "%";
				strTemp += szTemp;
				result += strTemp;
			}
		}
		else
		{
			sprintf_s(szTemp, "%02X", *(PBYTE)(buffer + i));
			strTemp = "%";
			strTemp += szTemp;
			result += strTemp;
		}
	}
	delete[] buffer;
	return result;
}

std::string WebHttpRequest::UnEscape(const char* strSource)
{
	std::string strResult;
	int nDestStep = 0;
	int nLength = strlen(strSource);
	if (!nLength || nLength < 6) return strResult;
	char* pResult = new char[nLength + 1];
	wchar_t* pWbuufer = nullptr;
	if (!pResult)
	{
		pResult = NULL;
		return strResult;
	}
	ZeroMemory(pResult, nLength + 1);
	for (int nPos = 0; nPos < nLength; nPos++)
	{
		if (strSource[nPos] == '\\' && strSource[nPos + 1] == 'u')
		{
			char szTemp[5];
			char szSource[5];
			ZeroMemory(szTemp, 5);
			ZeroMemory(szSource, 5);
			CopyMemory(szSource, (char*)strSource + nPos + 2, 4);
			sscanf_s(szSource, "%04X", szTemp);
			CopyMemory(pResult + nDestStep, szTemp, 4);
			nDestStep += 2;
		}
	}
	nDestStep += 2;
	pWbuufer = new wchar_t[nDestStep];
	if (!pWbuufer)
	{
		delete[] pWbuufer;
		pWbuufer = nullptr;
		return strResult;
	}
	ZeroMemory(pWbuufer, nDestStep);
	CopyMemory(pWbuufer, pResult, nDestStep);
	delete[] pResult;
	pResult = nullptr;
	CHAR* MultPtr = nullptr;
	int MultLen = -1;
	//GB2312_ACP = 936
	MultLen = ::WideCharToMultiByte(GB2312_ACP, WC_COMPOSITECHECK, pWbuufer, -1, NULL, NULL, NULL, NULL);
	MultPtr = new CHAR[MultLen + 1];
	if (MultPtr)
	{
		ZeroMemory(MultPtr, MultLen + 1);
		::WideCharToMultiByte(GB2312_ACP, WC_COMPOSITECHECK, pWbuufer, -1, MultPtr, MultLen, NULL, NULL);
		strResult = MultPtr;
		delete[] MultPtr;
		MultPtr = nullptr;
	}
	delete[] pWbuufer;
	pWbuufer = nullptr;
	return strResult;
}

std::string WebHttpRequest::Gbk_To_Utf8(const char* szBuff)
{
	std::string resault;
	int widLen = 0;
	int MultiLen = 0;
	WCHAR* widPtr = nullptr;
	CHAR* MulitPtr = nullptr;
	widLen = ::MultiByteToWideChar(CP_ACP, NULL, szBuff, -1, NULL, NULL);//获取转换后需要的空间
	widPtr = new WCHAR[widLen + 1];
	if (!widPtr)
		return resault;
	ZeroMemory(widPtr, (widLen + 1) * sizeof(WCHAR));
	if (!::MultiByteToWideChar(CP_ACP, NULL, szBuff, -1, widPtr, widLen))
	{
		delete[] widPtr;
		widPtr = nullptr;
		return resault;
	}
	MultiLen = ::WideCharToMultiByte(CP_UTF8, NULL, widPtr, -1, NULL, NULL, NULL, NULL);
	if (MultiLen)
	{
		MulitPtr = new CHAR[MultiLen + 1];
		if (!MulitPtr)
		{
			delete[] widPtr;
			widPtr = nullptr;
			return resault;
		}
		ZeroMemory(MulitPtr, (MultiLen + 1) * sizeof(CHAR));
		::WideCharToMultiByte(CP_UTF8, NULL, widPtr, -1, MulitPtr, MultiLen, NULL, NULL);
		resault = MulitPtr;
		delete[] MulitPtr;
		MulitPtr = nullptr;
	}
	delete[] widPtr;
	widPtr = nullptr;
	return resault;
}

std::string WebHttpRequest::Unicode_To_Ansi(const wchar_t* szbuff)
{
	std::string result;
	CHAR* MultPtr = nullptr;
	int MultLen = -1;
	MultLen = ::WideCharToMultiByte(GB2312_ACP, WC_COMPOSITECHECK, szbuff, -1, NULL, NULL, NULL, NULL);
	MultPtr = new CHAR[MultLen + 1];
	if (MultPtr)
	{
		ZeroMemory(MultPtr, MultLen + 1);
		::WideCharToMultiByte(GB2312_ACP, WC_COMPOSITECHECK, szbuff, -1, MultPtr, MultLen, NULL, NULL);
		result = MultPtr;
		delete[] MultPtr;
		MultPtr = nullptr;
	}
	return result;
}

std::wstring WebHttpRequest::Ansi_To_Unicode(const char* szbuff)
{
	std::wstring result;
	WCHAR* widePtr = nullptr;
	int wideLen = -1;
	wideLen = ::MultiByteToWideChar(CP_ACP, NULL, szbuff, -1, NULL, NULL);
	widePtr = new WCHAR[wideLen + 1];
	if (widePtr)
	{
		ZeroMemory(widePtr, (wideLen + 1) * sizeof(WCHAR));
		::MultiByteToWideChar(CP_ACP, NULL, szbuff, -1, widePtr, wideLen);
		result = widePtr;
		delete[] widePtr;
		widePtr = nullptr;
	}
	return result;
}

std::string WebHttpRequest::Utf8_To_Gbk(const char* szBuff)
{
	std::string resault;
	int widLen = 0;
	int MultiLen = 0;
	WCHAR* widPtr = nullptr;
	CHAR* MulitPtr = nullptr;
	widLen = ::MultiByteToWideChar(CP_UTF8, NULL, szBuff, -1, NULL, NULL);//获取转换后需要的空间
	widPtr = new WCHAR[widLen + 1];
	if (!widPtr)
		return resault;
	ZeroMemory(widPtr, (widLen + 1) * sizeof(WCHAR));
	if (!::MultiByteToWideChar(CP_UTF8, NULL, szBuff, -1, widPtr, widLen))
	{
		delete[] widPtr;
		widPtr = nullptr;
		return resault;
	}
	MultiLen = ::WideCharToMultiByte(CP_ACP, NULL, widPtr, -1, NULL, NULL, NULL, NULL);
	if (MultiLen)
	{
		MulitPtr = new CHAR[MultiLen + 1];
		if (!MulitPtr)
		{
			delete[] widPtr;
			widPtr = nullptr;
			return resault;
		}
		ZeroMemory(MulitPtr, (MultiLen + 1) * sizeof(CHAR));
		::WideCharToMultiByte(CP_ACP, NULL, widPtr, -1, MulitPtr, MultiLen, NULL, NULL);
		resault = MulitPtr;
		delete[] MulitPtr;
		MulitPtr = nullptr;
	}
	delete[] widPtr;
	widPtr = nullptr;
	return resault;
}

MEMORYBUF WebHttpRequest::GetStreamBuf(const std::string strUrl)
{
	CURLcode dwCurlCode;
	CURL* curl_Http_Ptr = nullptr;
	MEMORYBUF content;
	struct curl_slist* headerList = nullptr;	//自定义头文件
	dwCurlCode = curl_global_init(CURL_GLOBAL_ALL);	//初始化curl环境
	curl_Http_Ptr = curl_easy_init();	// 启动一个libcurl easy会话
	if (curl_Http_Ptr)
	{
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_URL, strUrl.c_str());//设置需要访问的网址
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_AUTOREFERER, 1);	//自动更新引用标题
		//将指向零终止字符串的指针作为参数传递。它将用于在HTTP请求中设置cookie。字符串的格式应该是NAME = CONTENTS，其中NAME是Cookie名称，CONTENTS是Cookie应该包含的内容。
		//如果您需要设置多个Cookie，请使用单个选项将它们全部设置为：“name1 = content1; name2 = content2; ” 等等
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_SSL_VERIFYPEER, false);	// 验证对方的SSL证书
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_SSL_VERIFYHOST, false);	//根据主机验证证书的名称
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_WRITEFUNCTION, write_image_data);  //设置写的回调函数
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_WRITEDATA, &content);	//接收返回的数据
		dwCurlCode = curl_easy_perform(curl_Http_Ptr);
		if (CURLE_OK != dwCurlCode)
		{
			const char * szError = nullptr;
			szError = curl_easy_strerror(dwCurlCode);
			int nLength = lstrlenA(szError);
			for (int i = 0; i < nLength; i++)
				content.push_back(szError[i]);
		}
		//printf("%s\r\n", CLuaParse::Utf8_To_Gbk(content.c_str()).c_str());
		curl_slist_free_all(headerList);	//释放 自定义请求头链表
		curl_easy_cleanup(curl_Http_Ptr);	//释放 libcurl easy会话
	}
	curl_global_cleanup();	//关闭curl环境
	return content;
}

std::string WebHttpRequest::Lua_Get_String(std::string strUrl, std::string& RetHeader, std::string strHeader /*= ""*/, std::string strCookies /*= ""*/)
{
	std::string strResult;
	HEADERLIST headerList;
	if (strUrl.empty()) {
		return strResult;
	}
	if (!strHeader.empty())
	{
		std::string split = strHeader + "\r\n";
		std::string strsub;
		size_t pos = 0;
		size_t npos = 0;
		npos = split.find("\r\n", pos);
		while (npos != std::string::npos)
		{
			strsub = split.substr(pos, npos - pos);
			pos = npos + strlen("\r\n");
			npos = split.find("\r\n", pos);
			headerList.push_back(strsub);
		}
	}
	REQUESTINFO UrlData;
	//UrlData.strData = data;
	UrlData.strUrl = strUrl;
	UrlData.strCookie = strCookies;
	UrlData.headerList = &headerList;
	strResult = Curl_get_data(UrlData, RetHeader);
	return strResult;
}

std::string WebHttpRequest::Lua_Post_String(std::string strUrl, std::string& RetHeader, std::string strSendData /*= ""*/, std::string strHeader /*= ""*/, std::string strCookies /*= ""*/)
{
	std::string strResult;
	HEADERLIST headerList;
	if (strUrl.empty()) {
		return strResult;
	}
	if (!strHeader.empty())
	{
		std::string split = strHeader + "\r\n";
		std::string strsub;
		size_t pos = 0;
		size_t npos = 0;
		npos = split.find("\r\n", pos);
		while (npos != std::string::npos)
		{
			strsub = split.substr(pos, npos - pos);
			pos = npos + strlen("\r\n");
			npos = split.find("\r\n", pos);
			headerList.push_back(strsub);
		}
	}
	REQUESTINFO UrlData;
	UrlData.strData = strSendData;
	UrlData.strUrl = strUrl;
	UrlData.strCookie = strCookies;
	UrlData.headerList = &headerList;
	strResult = Curl_post_data(UrlData, RetHeader);
	return strResult;
}

std::string WebHttpRequest::Curl_post_data(REQUESTINFO urlInfo, std::string& strRetHeader)
{
	CURLcode dwCurlCode;
	CURL* curl_Http_Ptr = nullptr;
	std::string content;
	struct curl_slist* headerList = nullptr;	//自定义头文件
	dwCurlCode = curl_global_init(CURL_GLOBAL_ALL);	//初始化curl环境
	curl_Http_Ptr = curl_easy_init();	// 启动一个libcurl easy会话
	if (curl_Http_Ptr)
	{
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_URL, urlInfo.strUrl.c_str());//设置需要访问的网址
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_AUTOREFERER, 1);	//自动更新引用标题
		for (HEADERLIST::iterator it = urlInfo.headerList->begin(); it != urlInfo.headerList->end(); ++it)
		{
			headerList = curl_slist_append(headerList, (*it).c_str());
		}
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_COOKIE, urlInfo.strCookie.c_str());	//设置cookie
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_HTTPHEADER, headerList);	//设置自定义协议头头
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_SSL_VERIFYPEER, false);	// 验证对方的SSL证书
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_SSL_VERIFYHOST, false);	//根据主机验证证书的名称
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_POSTFIELDSIZE, (long)urlInfo.strData.length());	//设置需要发送的数据大小
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_POSTFIELDS, urlInfo.strData.c_str());	//设置需要发送的数据
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_HEADERFUNCTION, header_callback);	//设置回调协议头函数
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_HEADERDATA, &strRetHeader);
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_WRITEFUNCTION, write_callback);  //设置写的回调函数
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_WRITEDATA, &content);	//接收返回的数据
		dwCurlCode = curl_easy_perform(curl_Http_Ptr);
		if (CURLE_OK != dwCurlCode)
		{
			content = curl_easy_strerror(dwCurlCode);
		}
		//printf("%s\r\n", CLuaParse::Utf8_To_Gbk(content.c_str()).c_str());
		curl_slist_free_all(headerList);	//释放 自定义请求头链表
		curl_easy_cleanup(curl_Http_Ptr);	//释放 libcurl easy会话
	}
	curl_global_cleanup();	//关闭curl环境
	return content;
}

std::string WebHttpRequest::Curl_get_data(REQUESTINFO urlInfo, std::string& strRetHeader)
{
	CURLcode dwCurlCode;
	CURL* curl_Http_Ptr = nullptr;
	std::string content;
	struct curl_slist* headerList = nullptr;	//自定义头文件
	dwCurlCode = curl_global_init(CURL_GLOBAL_ALL);	//初始化curl环境
	curl_Http_Ptr = curl_easy_init();	// 启动一个libcurl easy会话
	if (curl_Http_Ptr)
	{
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_URL, urlInfo.strUrl.c_str());//设置需要访问的网址
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_AUTOREFERER, 1);	//自动更新引用标题
		for (HEADERLIST::iterator it = urlInfo.headerList->begin(); it != urlInfo.headerList->end(); ++it)
		{
			headerList = curl_slist_append(headerList, (*it).c_str());
		}
		//将指向零终止字符串的指针作为参数传递。它将用于在HTTP请求中设置cookie。字符串的格式应该是NAME = CONTENTS，其中NAME是Cookie名称，CONTENTS是Cookie应该包含的内容。
		//如果您需要设置多个Cookie，请使用单个选项将它们全部设置为：“name1 = content1; name2 = content2; ” 等等
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_COOKIE, urlInfo.strCookie.c_str());	//设置cookie
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_HTTPHEADER, headerList);	//设置自定义协议头头
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_SSL_VERIFYPEER, false);	// 验证对方的SSL证书
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_SSL_VERIFYHOST, false);	//根据主机验证证书的名称
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_HEADERFUNCTION, header_callback);	//设置回调协议头函数
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_HEADERDATA, &strRetHeader);
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_WRITEFUNCTION, read_callback);  //设置写的回调函数
		curl_easy_setopt(curl_Http_Ptr, CURLOPT_WRITEDATA, &content);	//接收返回的数据
		dwCurlCode = curl_easy_perform(curl_Http_Ptr);
		if (CURLE_OK != dwCurlCode)
		{
			content = curl_easy_strerror(dwCurlCode);
		}
		//printf("%s\r\n", CLuaParse::Utf8_To_Gbk(content.c_str()).c_str());
		curl_slist_free_all(headerList);	//释放 自定义请求头链表
		curl_easy_cleanup(curl_Http_Ptr);	//释放 libcurl easy会话
	}
	curl_global_cleanup();	//关闭curl环境
	return content;
}

size_t WebHttpRequest::write_image_data(char *ptr, size_t size, size_t nmemb, MEMORYBUF* stream)
{
	size_t lsize = size * nmemb;
	for (size_t i = 0; i < lsize;i++)
		stream->push_back(ptr[i]);
	return lsize;
}

size_t WebHttpRequest::write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
	return written;
}

size_t WebHttpRequest::header_callback(char *buffer, size_t size, size_t nitems, std::string&userdata)
{
	// 回调回来的数据存在ptr指针中 这个数据的长度 是size * nmemb 的大小
	size_t rest = 0;
	LONG lsize = size * nitems;
	std::string stemp(buffer, lsize);
	userdata += stemp;
	return lsize;
}

size_t WebHttpRequest::write_callback(char *ptr, size_t size, size_t nmemb, std::string& userdata)
{
	// 回调回来的数据存在ptr指针中 这个数据的长度 是size * nmemb 的大小
	size_t rest = 0;
	LONG lsize = size * nmemb;
	std::string stemp(ptr, lsize);
	userdata += stemp;
	return lsize;
}

size_t WebHttpRequest::read_callback(char *ptr, size_t size, size_t nmemb, std::string& userdata)
{
	// 回调回来的数据存在ptr指针中 这个数据的长度 是size * nmemb 的大小
	size_t rest = 0;
	LONG lsize = size * nmemb;
	std::string stemp(ptr, lsize);
	userdata += stemp;
	return lsize;
}
