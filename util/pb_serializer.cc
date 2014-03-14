// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#include <google/protobuf/message.h>
#include <unordered_map>
#include <iostream>

#include "base/googleinit.h"
#include "util/coding/pb_writer.h"
#include "util/coding/pb_reader.h"
#include "util/plang/addressbook.pb.h"
#include "util/sinksource.h"

using namespace tutorial;
using namespace std;

using namespace util::coding;

int main(int argc, char **argv) {
  MainInitGuard guard(&argc, &argv);

  const gpb::Descriptor* descr = BankAccount::descriptor();
  std::vector<const gpb::FieldDescriptor*> required, optional, repeated;
  //const gpb::Reflection* refl = BankAccount::default_instance().GetReflection();

  for (int i = 0; i < descr->field_count(); ++i) {
    const gpb::FieldDescriptor* fd = descr->field(i);
    cout << fd->full_name() << endl;
  }
  PbBlockSerializer writer(descr);
  BankAccount ba;
  for (int i = 0; i < 10; ++i) {
    ba.add_activity_id(i*5);
  }
  ba.set_bank_name("leumi");
  ba.mutable_address()->set_street("forrest st");
  unsigned kNumMsg = 20;
  for (unsigned j = 0; j < kNumMsg; ++j)
    writer.Add(ba);
  util::StringSink ssink;
  writer.SerializeTo(&ssink);
  cout << "PB: " << ba.ByteSize() << ", serialized: " << ssink.contents().size() << endl;
  cout << writer.DebugStats() << endl;
  PbBlockDeserializer deserializer(descr);
  uint32 num_msgs = 0;
  util::Status status = deserializer.Init(ssink.contents(), &num_msgs);
  CHECK(status.ok()) << status;
  CHECK_EQ(kNumMsg, num_msgs);
  for (uint32 i = 0; i < num_msgs; ++i) {
    BankAccount dest;
    status = deserializer.Read(&dest);
    cout << dest.ShortDebugString() << endl;
    CHECK(status.ok()) << i;
  }
  return 0;
}