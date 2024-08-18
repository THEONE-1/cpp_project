#include <iostream>
#include <signal.h>
#include <nlohmann/json.hpp>
#include <workflow/Workflow.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFGlobal.h>
#include <workflow/WFHttpServer.h>
#include <workflow/MySQLResult.h>
#include <workflow/HttpMessage.h>
#include <workflow/RedisMessage.h>
#include <workflow/WFTaskFactory.h>
#include <wfrest/HttpServer.h>
#include <wfrest/json.hpp>
#include <wfrest/MysqlUtil.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <memory>
#include <cstring>
#include <crypt.h>
#include<functional>
#include "FileUtil.hpp"

using namespace protocol;
using namespace std;
using namespace placeholders;
using Json = nlohmann::json;
