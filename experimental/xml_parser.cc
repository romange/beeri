#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_print.hpp>
#include <iostream>

#include "base/googleinit.h"
#include "util/http/http_client.h"

using namespace rapidxml;
int main(int argc, char **argv) {
  MainInitGuard guard(&argc, &argv);
  http::HttpClient client;

  string res;
  CHECK(client.ReadToString("http://www.openstreetmap.org/api/0.6/way/42062595/full", &res).ok());
  xml_document<> doc;
  doc.parse<0>(&res.front());
  xml_node<>* osm_root = doc.first_node("osm");
  for (xml_node<>* node = osm_root->first_node("node"); node != nullptr;
       node = node->next_sibling("node")) {
    xml_attribute<>* id = node->first_attribute();
    std::cout << id->value() << std::endl;
  }

  return 0;
}