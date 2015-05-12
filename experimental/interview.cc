// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "base/googleinit.h"

// Could be found in /usr/include/cppconn/driver.h
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

using namespace std;

int main(int argc, char **argv) {
  MainInitGuard guard(&argc, &argv);

  sql::Driver * driver = get_driver_instance();
  try {
    std::unique_ptr<sql::Connection> con(driver->connect("tcp://localhost:3306", "root", "test"));
    con->setSchema("interview");
    std::unique_ptr<sql::Statement> statement(con->createStatement());
    
    CHECK(statement->execute("SHOW TABLES"));
    std::unique_ptr<sql::ResultSet > res(statement->getResultSet());
    sql::ResultSetMetaData* res_meta = res->getMetaData();
    CHECK_EQ(1, res_meta->getColumnCount());
    cout << res_meta->getColumnName(1) << ":\n";
    while (res->next()) {
      cout << " table = " << res->getString(1) << endl;
    }
  } catch (sql::SQLException& e) {
    LOG(ERROR) << "Mysql error " << e.getErrorCode() << ", msg: " << e.what();
  }
 
  return 0;
}
