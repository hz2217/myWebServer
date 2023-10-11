#include "httpresponse.h"

using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css" },
    { ".js",    "text/javascript" },
};

const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse() {
	code_ = -1;
	path_ = srcDir_ = "";
	isKeepAlive_ = false;
	mmFile_ = nullptr;
	mmFileStat_ = { 0 };
}

HttpResponse::~HttpResponse() {
	UnmapFile();
}

void HttpResponse::Init(const string &srcDir, string &path, bool isKeepAlive, int code) {
	assert(srcDir != "");
	if (mmFile_) { UnmapFile(); }
	code_ = code;
	isKeepAlive_ = isKeepAlive;
	path_ = path;
	srcDir_ = srcDir;
	mmFile_ = nullptr;
	mmFileStat_ = { 0 };
}

void HttpResponse::MakeResponse(Buffer &buffer) {
	// stat 通过文件名 filename 获取文件信息，例如文件描述符和文件大小
	// 若文件不存在 | 是个目录
	if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
		code_ = 404;
	}
	//  S_IROTH 00004 其他用户具可读取权限
	//  没有可读权限
	else if (!(mmFileStat_.st_mode & S_IROTH)) {
		code_ = 403;
	}
	else if (code_ == -1) {
		code_ = 200;
	}
	// 400/403/404 三种错误情况页面
	ErrorHtml_();
	// 其他正常页面 add line header content
	AddStateLine_(buffer);
	AddHeader_(buffer);
	AddContent_(buffer);
}

char* HttpResponse::File() {
	return mmFile_;
}

size_t HttpResponse::FileLen() const {
	return mmFileStat_.st_size;
}

void HttpResponse::ErrorHtml_() {
	if (CODE_PATH.count(code_) == 1) {
		path_ = CODE_PATH.find(code_)->second;
		stat((srcDir_ + path_).data(), &mmFileStat_);
	}
}

void HttpResponse::AddStateLine_(Buffer &buffer) {
	string status;
	if (CODE_STATUS.count(code_) == 1) {
		status = CODE_STATUS.find(code_)->second;	
	}
	else {
		code_ = 400;
		status = CODE_STATUS.find(400)->second;
	}
	buffer.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader_(Buffer &buffer) {
	buffer.Append("Connection: ");
	if (isKeepAlive_) {
		buffer.Append("keep-alive\r\n");
		buffer.Append("keep-alive: max=6, timeout=120\r\n");
	} else {
		buffer.Append("close\r\n");
	}
	buffer.Append("Content-type: " + GetFileType_() + "\r\n");
}

void HttpResponse::AddContent_(Buffer &buffer) {
	int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
	if (srcFd < 0) {
		ErrorContent(buffer, "File NotFound!");
		return;
	}
	
	// 1. 将文件映射到内存
	// 2. MAP_PRIVATE 建立一个写入时拷贝的私有映射
	LOG_DEBUG("file path %s", (srcDir_ + path_).data());	
	int *mmRet = (int *)mmap(NULL, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
	if (*mmRet == -1) {
		ErrorContent(buffer, "File NotFound!");
		return;
	}
	mmFile_ = (char *)mmRet;
	close(srcFd);
	buffer.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

void HttpResponse::UnmapFile() {
	if (mmFile_) {
		munmap(mmFile_, mmFileStat_.st_size);
		mmFile_ = nullptr;
	}
}

string HttpResponse::GetFileType_() {
	string::size_type idx = path_.find_last_of('.');
	if (idx == string::npos) {
		return "text/plain";
	}
	string suffix = path_.substr(idx);
	if (SUFFIX_TYPE.count(suffix) == 1) {
		return SUFFIX_TYPE.find(suffix)->second;
	}
	return "text/plain";
}

void HttpResponse::ErrorContent(Buffer &buffer, string message) {
	string body;
	string status;
	body += "<html><title>Error</title>";
	body += "<body bgcolor=\"ffffff\">";
	if (CODE_STATUS.count(code_) == 1) {
		status = CODE_STATUS.find(code_)->second;
	} else {
		status = "Bad Request";
	}
	body += to_string(code_) + " : " + status + "\n";
	body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buffer.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buffer.Append(body);	
}









