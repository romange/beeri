// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include <gperftools/malloc_extension.h>
#include <gperftools/profiler.h>

#include "base/walltime.h"
#include "strings/numbers.h"
#include "strings/split.h"
#include "util/http/http_server.h"

namespace http {
namespace {
char last_profile_suffix[100] = {0};
}

namespace internal {

void ProfilezHandler(const Request& request, Response* response) {
  VLOG(1) << "query: " << request.query();
  response->SetContentType(Response::kHtmlMime);
  Request::KeyValueArray args = request.ParsedQuery();
  bool pass = true;
  bool enable = false;
  std::unique_ptr<char[]> mem_stats;
  for (const auto& k_v : args) {
    if (k_v.first == "profile" && k_v.second == "on") {
      enable = true;
    } else if (k_v.first == "mem") {
      mem_stats.reset(new char[1024]);
      MallocExtension::instance()->GetStats(mem_stats.get(), 1024);
    }
  }
  if (!pass) {
    response->Send(HTTP_UNAUTHORIZED);
    return;
  }
  response->AppendContent(R"(<!DOCTYPE html>
    <html>
      <head> <title>Profilez</title> </head>
      <body>)");
  string profile_name = "/tmp/" + base::ProgramBaseName();
  if (mem_stats) {
    response->AppendContent("<pre>").AppendContent(mem_stats.get()).AppendContent("</pre>");
  } else {
    if (enable) {
      if (last_profile_suffix[0]) {
        response->AppendContent("<p> Yo, already profiling, stupid!</p>\n");
      } else {
        string suffix = LocalTimeNow("_%d%m%Y_%H%M%S.prof");
        profile_name.append(suffix);
        strcpy(last_profile_suffix, suffix.c_str());
        int res = ProfilerStart(profile_name.c_str());
        LOG(INFO) << "Starting profiling into " << profile_name << " " << res;
        response->AppendContent("<p> Yeah, let's profile this bitch, baby!</p> \n"
          "<img src='//super3s.com/files/2012/12/weasel_with_hula_hoop_hc-23g0lmj.gif'>\n");
      }
    } else {
      ProfilerStop();
      if (last_profile_suffix[0]) {
        string cmd("nice -n 15 pprof --svg ");
        string symbols_name = base::ProgramAbsoluteFileName() + ".debug";
        LOG(INFO) << "Symbols " << symbols_name << ", suffix: " << last_profile_suffix;
        if (access(symbols_name.c_str(), R_OK) != 0) {
          symbols_name = base::ProgramAbsoluteFileName();
        }
        cmd.append(symbols_name).append(" ");

        profile_name.append(last_profile_suffix);
        cmd.append(profile_name).append(" > ");

        profile_name.append(".svg");
        cmd.append(profile_name);

        LOG(INFO) << "Running command: " << cmd;

        system(cmd.c_str());

        // Redirect browser to show this file.
        string url("/filez?file=");
        url.append(profile_name);
        response->AddHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->AddHeader("Pragma", "no-cache");
        response->AddHeader("Expires", "0");
        response->AddHeaderCopy("Location", url.c_str());
        response->Send(HTTP_MOVED_PERMANENTLY);
        last_profile_suffix[0] = '\0';
        return;
      }
      response->AppendContent("<h3>Profiling is off, commander!</h3> \n");
    }
  }
  response->AppendContent("</body> </html>\n");
  response->Send(HTTP_OK);
}

void FilezHandler(const Request& request, Response* response) {
  Request::KeyValueArray args = request.ParsedQuery();
  bool pass = true;
  StringPiece file_name;
  for (const auto& k_v : args) {
    if (k_v.first == "file") {
      file_name = k_v.second;
    }
  }
  if (!pass || file_name.empty()) {
    response->Send(HTTP_UNAUTHORIZED);
    return;
  }
  if (file_name.ends_with(".svg")) {
    response->SetContentType("image/svg+xml");
  } else if (file_name.ends_with(".html")) {
    response->SetContentType(Response::kHtmlMime);
  } else {
    response->SetContentType(Response::kTextMime);
  }
  response->SendFile(file_name.data(), HTTP_OK);
}

void FlagzHandler(const Request& request, Response* response) {
  Request::KeyValueArray args = request.ParsedQuery();
  bool pass = true;
  StringPiece flag;
  StringPiece value;
  for (const auto& k_v : args) {
    if (k_v.first == "flag") {
      flag = k_v.second;
    } else if (k_v.first == "value") {
      value = k_v.second;
    }
  }
  if (!pass || flag.empty()) {
    response->Send(HTTP_UNAUTHORIZED);
    return;
  }
  google::CommandLineFlagInfo flag_info;
  if (!google::GetCommandLineFlagInfo(flag.data(), &flag_info)) {
    response->AppendContent("Flag not found \n");
  } else {
    response->SetContentType(Response::kHtmlMime);
    response->AppendContent("<p>Current value ").AppendContent(flag_info.current_value)
        .AppendContent("</p>");
    string res = google::SetCommandLineOption(flag.data(), value.data());
    response->AppendContent("Flag ").AppendContent(res);
    if (flag == "vmodule") {
      vector<StringPiece> parts = strings::Split(value, ",", strings::SkipEmpty());
      for (StringPiece p : parts) {
        size_t sep = p.find('=');
        int32 level = 0;
        if (sep != StringPiece::npos && safe_strto32(p.substr(sep + 1), &level)) {
          string module_expr = p.substr(0, sep).as_string();
          LOG(INFO) << "Setting module " << module_expr << " to loglevel " << level;
          google::SetVLOGLevel(module_expr.c_str(), level);
        }
      }
    }
  }

  response->Send(HTTP_OK);
}

}  // namespace internal
}  // namespace http