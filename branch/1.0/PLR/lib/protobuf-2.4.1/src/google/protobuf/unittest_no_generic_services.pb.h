// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: google/protobuf/unittest_no_generic_services.proto

#ifndef PROTOBUF_google_2fprotobuf_2funittest_5fno_5fgeneric_5fservices_2eproto__INCLUDED
#define PROTOBUF_google_2fprotobuf_2funittest_5fno_5fgeneric_5fservices_2eproto__INCLUDED

#include <string>

#include <google/protobuf/stubs/common.h>

#if GOOGLE_PROTOBUF_VERSION < 2004000
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please update
#error your headers.
#endif
#if 2004001 < GOOGLE_PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/generated_message_reflection.h>
// @@protoc_insertion_point(includes)

namespace google {
namespace protobuf {
namespace no_generic_services_test {

// Internal implementation detail -- do not call these.
void  protobuf_AddDesc_google_2fprotobuf_2funittest_5fno_5fgeneric_5fservices_2eproto();
void protobuf_AssignDesc_google_2fprotobuf_2funittest_5fno_5fgeneric_5fservices_2eproto();
void protobuf_ShutdownFile_google_2fprotobuf_2funittest_5fno_5fgeneric_5fservices_2eproto();

class TestMessage;

enum TestEnum {
  FOO = 1
};
bool TestEnum_IsValid(int value);
const TestEnum TestEnum_MIN = FOO;
const TestEnum TestEnum_MAX = FOO;
const int TestEnum_ARRAYSIZE = TestEnum_MAX + 1;

const ::google::protobuf::EnumDescriptor* TestEnum_descriptor();
inline const ::std::string& TestEnum_Name(TestEnum value) {
  return ::google::protobuf::internal::NameOfEnum(
    TestEnum_descriptor(), value);
}
inline bool TestEnum_Parse(
    const ::std::string& name, TestEnum* value) {
  return ::google::protobuf::internal::ParseNamedEnum<TestEnum>(
    TestEnum_descriptor(), name, value);
}
// ===================================================================

class TestMessage : public ::google::protobuf::Message {
 public:
  TestMessage();
  virtual ~TestMessage();
  
  TestMessage(const TestMessage& from);
  
  inline TestMessage& operator=(const TestMessage& from) {
    CopyFrom(from);
    return *this;
  }
  
  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const {
    return _unknown_fields_;
  }
  
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields() {
    return &_unknown_fields_;
  }
  
  static const ::google::protobuf::Descriptor* descriptor();
  static const TestMessage& default_instance();
  
  void Swap(TestMessage* other);
  
  // implements Message ----------------------------------------------
  
  TestMessage* New() const;
  void CopyFrom(const ::google::protobuf::Message& from);
  void MergeFrom(const ::google::protobuf::Message& from);
  void CopyFrom(const TestMessage& from);
  void MergeFrom(const TestMessage& from);
  void Clear();
  bool IsInitialized() const;
  
  int ByteSize() const;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input);
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const;
  ::google::protobuf::uint8* SerializeWithCachedSizesToArray(::google::protobuf::uint8* output) const;
  int GetCachedSize() const { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  public:
  
  ::google::protobuf::Metadata GetMetadata() const;
  
  // nested types ----------------------------------------------------
  
  // accessors -------------------------------------------------------
  
  // optional int32 a = 1;
  inline bool has_a() const;
  inline void clear_a();
  static const int kAFieldNumber = 1;
  inline ::google::protobuf::int32 a() const;
  inline void set_a(::google::protobuf::int32 value);
  
  GOOGLE_PROTOBUF_EXTENSION_ACCESSORS(TestMessage)
  // @@protoc_insertion_point(class_scope:google.protobuf.no_generic_services_test.TestMessage)
 private:
  inline void set_has_a();
  inline void clear_has_a();
  
  ::google::protobuf::internal::ExtensionSet _extensions_;
  
  ::google::protobuf::UnknownFieldSet _unknown_fields_;
  
  ::google::protobuf::int32 a_;
  
  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(1 + 31) / 32];
  
  friend void  protobuf_AddDesc_google_2fprotobuf_2funittest_5fno_5fgeneric_5fservices_2eproto();
  friend void protobuf_AssignDesc_google_2fprotobuf_2funittest_5fno_5fgeneric_5fservices_2eproto();
  friend void protobuf_ShutdownFile_google_2fprotobuf_2funittest_5fno_5fgeneric_5fservices_2eproto();
  
  void InitAsDefaultInstance();
  static TestMessage* default_instance_;
};
// ===================================================================

static const int kTestExtensionFieldNumber = 1000;
extern ::google::protobuf::internal::ExtensionIdentifier< ::google::protobuf::no_generic_services_test::TestMessage,
    ::google::protobuf::internal::PrimitiveTypeTraits< ::google::protobuf::int32 >, 5, false >
  test_extension;

// ===================================================================

// TestMessage

// optional int32 a = 1;
inline bool TestMessage::has_a() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void TestMessage::set_has_a() {
  _has_bits_[0] |= 0x00000001u;
}
inline void TestMessage::clear_has_a() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void TestMessage::clear_a() {
  a_ = 0;
  clear_has_a();
}
inline ::google::protobuf::int32 TestMessage::a() const {
  return a_;
}
inline void TestMessage::set_a(::google::protobuf::int32 value) {
  set_has_a();
  a_ = value;
}


// @@protoc_insertion_point(namespace_scope)

}  // namespace no_generic_services_test
}  // namespace protobuf
}  // namespace google

#ifndef SWIG
namespace google {
namespace protobuf {

template <>
inline const EnumDescriptor* GetEnumDescriptor< google::protobuf::no_generic_services_test::TestEnum>() {
  return google::protobuf::no_generic_services_test::TestEnum_descriptor();
}

}  // namespace google
}  // namespace protobuf
#endif  // SWIG

// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_google_2fprotobuf_2funittest_5fno_5fgeneric_5fservices_2eproto__INCLUDED
