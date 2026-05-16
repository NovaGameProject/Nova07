// Nova Game Engine - Replication System Tests
// Tests packet serialization roundtrips, protocol correctness, and client-server parity.

#include "Engine/Networking/ReplicationProtocol.hpp"
#include "Engine/Networking/NetworkID.hpp"
#include "Common/PropertyValue.hpp"
#include "Common/MathTypes.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

using namespace Nova;

static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) \
    static void test_##name(); \
    struct TestRunner_##name { TestRunner_##name() { \
        printf("  %-50s", #name); \
        test_##name(); \
    }} runner_##name; \
    static void test_##name()

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL\n    %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
        testsFailed++; return; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, eps) do { \
    if (std::fabs((a) - (b)) > (eps)) { \
        printf("FAIL\n    %s:%d: %s (%f) !~= %s (%f)\n", __FILE__, __LINE__, #a, (double)(a), #b, (double)(b)); \
        testsFailed++; return; \
    } \
} while(0)

#define ASSERT_TRUE(x) do { \
    if (!(x)) { \
        printf("FAIL\n    %s:%d: %s\n", __FILE__, __LINE__, #x); \
        testsFailed++; return; \
    } \
} while(0)

#define PASS() do { printf("OK\n"); testsPassed++; } while(0)

// ===== PacketWriter/PacketReader roundtrip tests =====

TEST(writer_reader_u8) {
    PacketWriter w;
    w.WriteU8(0);
    w.WriteU8(255);
    w.WriteU8(42);
    PacketReader r(w.GetData().data(), w.GetData().size());
    ASSERT_EQ(r.ReadU8(), 0);
    ASSERT_EQ(r.ReadU8(), 255);
    ASSERT_EQ(r.ReadU8(), 42);
    PASS();
}

TEST(writer_reader_u16) {
    PacketWriter w;
    w.WriteU16(0);
    w.WriteU16(65535);
    w.WriteU16(1234);
    PacketReader r(w.GetData().data(), w.GetData().size());
    ASSERT_EQ(r.ReadU16(), 0);
    ASSERT_EQ(r.ReadU16(), 65535);
    ASSERT_EQ(r.ReadU16(), 1234);
    PASS();
}

TEST(writer_reader_u32) {
    PacketWriter w;
    w.WriteU32(0);
    w.WriteU32(0xFFFFFFFF);
    w.WriteU32(123456789);
    PacketReader r(w.GetData().data(), w.GetData().size());
    ASSERT_EQ(r.ReadU32(), 0u);
    ASSERT_EQ(r.ReadU32(), 0xFFFFFFFF);
    ASSERT_EQ(r.ReadU32(), 123456789u);
    PASS();
}

TEST(writer_reader_float) {
    PacketWriter w;
    w.WriteFloat(0.0f);
    w.WriteFloat(3.14159f);
    w.WriteFloat(-1.5f);
    PacketReader r(w.GetData().data(), w.GetData().size());
    ASSERT_NEAR(r.ReadFloat(), 0.0f, 0.001f);
    ASSERT_NEAR(r.ReadFloat(), 3.14159f, 0.001f);
    ASSERT_NEAR(r.ReadFloat(), -1.5f, 0.001f);
    PASS();
}

TEST(writer_reader_string) {
    PacketWriter w;
    w.WriteString("");
    w.WriteString("Hello");
    w.WriteString(std::string(1000, 'A'));
    PacketReader r(w.GetData().data(), w.GetData().size());
    ASSERT_EQ(r.ReadString(), "");
    ASSERT_EQ(r.ReadString(), "Hello");
    ASSERT_EQ(r.ReadString(), std::string(1000, 'A'));
    PASS();
}

TEST(writer_reader_vec3) {
    PacketWriter w;
    Vector3 v = {1.5f, -2.3f, 99.0f};
    w.WriteVec3(v);
    PacketReader r(w.GetData().data(), w.GetData().size());
    Vector3 out = r.ReadVec3();
    ASSERT_NEAR(out.x, 1.5f, 0.001f);
    ASSERT_NEAR(out.y, -2.3f, 0.001f);
    ASSERT_NEAR(out.z, 99.0f, 0.001f);
    PASS();
}

TEST(reader_bounds_check) {
    uint8_t data[] = {0x01, 0x02};
    PacketReader r(data, 2);
    ASSERT_EQ(r.ReadU8(), 1);
    ASSERT_EQ(r.ReadU8(), 2);
    // Out of bounds should return 0, not crash
    ASSERT_EQ(r.ReadU8(), 0);
    ASSERT_EQ(r.ReadU32(), 0u);
    ASSERT_EQ(r.ReadString(), "");
    PASS();
}

TEST(packet_writer_size) {
    PacketWriter w;
    ASSERT_EQ(w.Size(), 0u);
    w.WriteU8(0xFF);
    ASSERT_EQ(w.Size(), 1u);
    w.WriteU32(0x12345678);
    ASSERT_EQ(w.Size(), 5u);
    PASS();
}

// ===== PropertyValue serialization roundtrip tests =====

static PropertyValue roundtrip(const PropertyValue& v) {
    PacketWriter w;
    // Inline WritePropertyValue
    w.WriteU8(static_cast<uint8_t>(v.kind));
    switch (v.kind) {
        case PropertyValue::Kind::Nil: break;
        case PropertyValue::Kind::Bool: w.WriteU8(v.toBool() ? 1 : 0); break;
        case PropertyValue::Kind::Int: w.WriteU32(static_cast<uint32_t>(static_cast<int32_t>(v.toInt()))); break;
        case PropertyValue::Kind::Float: w.WriteFloat(static_cast<float>(v.toFloat())); break;
        case PropertyValue::Kind::String: w.WriteString(v.toString()); break;
        case PropertyValue::Kind::Vector3: w.WriteVec3(v.toVector3()); break;
        case PropertyValue::Kind::CFrame: {
            auto cf = v.toCFrame();
            w.WriteVec3(cf.position);
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    w.WriteFloat(cf.rotation[i][j]);
            break;
        }
        case PropertyValue::Kind::Color3: {
            auto c = v.toColor3();
            w.WriteFloat(c.r); w.WriteFloat(c.g); w.WriteFloat(c.b);
            break;
        }
    }
    // Inline ReadPropertyValue
    PacketReader r(w.GetData().data(), w.GetData().size());
    uint8_t kind = r.ReadU8();
    switch (static_cast<PropertyValue::Kind>(kind)) {
        case PropertyValue::Kind::Nil: return PropertyValue();
        case PropertyValue::Kind::Bool: return PropertyValue(r.ReadU8() != 0);
        case PropertyValue::Kind::Int: return PropertyValue(static_cast<int64_t>(static_cast<int32_t>(r.ReadU32())));
        case PropertyValue::Kind::Float: return PropertyValue(static_cast<double>(r.ReadFloat()));
        case PropertyValue::Kind::String: return PropertyValue(r.ReadString());
        case PropertyValue::Kind::Vector3: return PropertyValue(r.ReadVec3());
        case PropertyValue::Kind::CFrame: {
            CFrame cf;
            cf.position = r.ReadVec3();
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    cf.rotation[i][j] = r.ReadFloat();
            return PropertyValue(cf);
        }
        case PropertyValue::Kind::Color3: {
            Color3 c; c.r = r.ReadFloat(); c.g = r.ReadFloat(); c.b = r.ReadFloat();
            return PropertyValue::FromColor3(c);
        }
        default: return PropertyValue();
    }
}

TEST(property_nil_roundtrip) {
    PropertyValue v;
    auto out = roundtrip(v);
    ASSERT_TRUE(out.isNil());
    PASS();
}

TEST(property_bool_roundtrip) {
    PropertyValue v(true);
    auto out = roundtrip(v);
    ASSERT_TRUE(out.isBool());
    ASSERT_TRUE(out.toBool());
    PropertyValue v2(false);
    auto out2 = roundtrip(v2);
    ASSERT_TRUE(out2.isBool());
    ASSERT_TRUE(!out2.toBool());
    PASS();
}

TEST(property_int_roundtrip) {
    PropertyValue v(int64_t(42));
    auto out = roundtrip(v);
    ASSERT_TRUE(out.isInt());
    ASSERT_EQ(out.toInt(), 42);
    PASS();
}

TEST(property_int_negative_roundtrip) {
    PropertyValue v(int64_t(-100));
    auto out = roundtrip(v);
    ASSERT_TRUE(out.isInt());
    ASSERT_EQ(out.toInt(), -100);
    PASS();
}

TEST(property_float_roundtrip) {
    PropertyValue v(3.14);
    auto out = roundtrip(v);
    ASSERT_TRUE(out.isFloat());
    ASSERT_NEAR(out.toFloat(), 3.14, 0.01);
    PASS();
}

TEST(property_string_roundtrip) {
    PropertyValue v(std::string("hello world"));
    auto out = roundtrip(v);
    ASSERT_TRUE(out.isString());
    ASSERT_EQ(out.toString(), "hello world");
    PASS();
}

TEST(property_vector3_roundtrip) {
    PropertyValue v(Vector3{1.0f, 2.0f, 3.0f});
    auto out = roundtrip(v);
    ASSERT_TRUE(out.isVector3());
    ASSERT_NEAR(out.toVector3().x, 1.0f, 0.001f);
    ASSERT_NEAR(out.toVector3().y, 2.0f, 0.001f);
    ASSERT_NEAR(out.toVector3().z, 3.0f, 0.001f);
    PASS();
}

TEST(property_cframe_roundtrip) {
    CFrame cf;
    cf.position = {10.0f, 20.0f, 30.0f};
    cf.rotation = glm::mat3(1.0f); // identity
    PropertyValue v(cf);
    auto out = roundtrip(v);
    ASSERT_TRUE(out.isCFrame());
    auto outCF = out.toCFrame();
    ASSERT_NEAR(outCF.position.x, 10.0f, 0.001f);
    ASSERT_NEAR(outCF.position.y, 20.0f, 0.001f);
    ASSERT_NEAR(outCF.position.z, 30.0f, 0.001f);
    // Check rotation is identity
    ASSERT_NEAR(outCF.rotation[0][0], 1.0f, 0.001f);
    ASSERT_NEAR(outCF.rotation[1][1], 1.0f, 0.001f);
    ASSERT_NEAR(outCF.rotation[2][2], 1.0f, 0.001f);
    PASS();
}

TEST(property_color3_roundtrip) {
    auto v = PropertyValue::FromColor3(Color3(0.5f, 0.7f, 0.9f));
    auto out = roundtrip(v);
    ASSERT_TRUE(out.isColor3());
    auto c = out.toColor3();
    ASSERT_NEAR(c.r, 0.5f, 0.001f);
    ASSERT_NEAR(c.g, 0.7f, 0.001f);
    ASSERT_NEAR(c.b, 0.9f, 0.001f);
    PASS();
}

// ===== NetworkID Registry tests =====

TEST(network_id_allocate) {
    NetworkIDRegistry reg;
    ASSERT_EQ(reg.Allocate(), 1u);
    ASSERT_EQ(reg.Allocate(), 2u);
    ASSERT_EQ(reg.Allocate(), 3u);
    PASS();
}

TEST(network_id_not_found) {
    NetworkIDRegistry reg;
    ASSERT_TRUE(!reg.HasInstance(nullptr));
    ASSERT_TRUE(!reg.HasID(999));
    ASSERT_EQ(reg.GetInstance(999), nullptr);
    ASSERT_EQ(reg.GetNetworkID(nullptr), 0u);
    PASS();
}

TEST(network_id_clear) {
    NetworkIDRegistry reg;
    ASSERT_EQ(reg.Allocate(), 1u);
    ASSERT_EQ(reg.Allocate(), 2u);
    reg.Clear();
    ASSERT_TRUE(!reg.HasID(1));
    ASSERT_TRUE(!reg.HasID(2));
    // After clear, Allocate continues from where it left off
    ASSERT_EQ(reg.Allocate(), 3u);
    PASS();
}

// ===== PacketType enum tests =====

TEST(packet_type_values) {
    // Ensure packet types are distinct and within uint8_t range
    ASSERT_TRUE(static_cast<uint8_t>(PacketType::CreateObject) == 1);
    ASSERT_TRUE(static_cast<uint8_t>(PacketType::DestroyObject) == 2);
    ASSERT_TRUE(static_cast<uint8_t>(PacketType::PropertyUpdate) == 3);
    ASSERT_TRUE(static_cast<uint8_t>(PacketType::FullSync) == 4);
    ASSERT_TRUE(static_cast<uint8_t>(PacketType::BatchPropertyUpdate) == 10);
    PASS();
}

// ===== Batch property packet format test =====

TEST(batch_property_packet_format) {
    // Simulate what SendBatchProperties does
    PacketWriter writer;
    NetworkID id = 42;
    writer.WriteU8(static_cast<uint8_t>(PacketType::BatchPropertyUpdate));
    writer.WriteU32(id);

    std::vector<std::pair<std::string, PropertyValue>> properties;
    properties.emplace_back("CFrame", PropertyValue(CFrame{{1,2,3}, glm::mat3(1.0f)}));
    properties.emplace_back("Size", PropertyValue(Vector3{4, 1.2f, 2}));
    properties.emplace_back("Anchored", PropertyValue(true));

    writer.WriteU16(static_cast<uint16_t>(properties.size()));
    for (auto& [name, value] : properties) {
        writer.WriteString(name);
        // Inline WritePropertyValue
        writer.WriteU8(static_cast<uint8_t>(value.kind));
        switch (value.kind) {
            case PropertyValue::Kind::Bool: writer.WriteU8(value.toBool() ? 1 : 0); break;
            case PropertyValue::Kind::Vector3: writer.WriteVec3(value.toVector3()); break;
            case PropertyValue::Kind::CFrame: {
                auto cf = value.toCFrame();
                writer.WriteVec3(cf.position);
                for (int i = 0; i < 3; i++)
                    for (int j = 0; j < 3; j++)
                        writer.WriteFloat(cf.rotation[i][j]);
                break;
            }
            default: break;
        }
    }

    // Parse it back
    PacketReader r(writer.GetData().data(), writer.GetData().size());
    auto type = static_cast<PacketType>(r.ReadU8());
    ASSERT_EQ(type, PacketType::BatchPropertyUpdate);
    ASSERT_EQ(r.ReadU32(), 42u);
    uint16_t count = r.ReadU16();
    ASSERT_EQ(count, 3);

    // First property: CFrame
    std::string name0 = r.ReadString();
    ASSERT_EQ(name0, "CFrame");
    uint8_t kind0 = r.ReadU8();
    ASSERT_EQ(kind0, static_cast<uint8_t>(PropertyValue::Kind::CFrame));
    Vector3 pos = r.ReadVec3();
    ASSERT_NEAR(pos.x, 1.0f, 0.001f);
    ASSERT_NEAR(pos.y, 2.0f, 0.001f);
    ASSERT_NEAR(pos.z, 3.0f, 0.001f);
    for (int i = 0; i < 9; i++) r.ReadFloat(); // skip rotation

    // Second property: Size
    std::string name1 = r.ReadString();
    ASSERT_EQ(name1, "Size");
    uint8_t kind1 = r.ReadU8();
    ASSERT_EQ(kind1, static_cast<uint8_t>(PropertyValue::Kind::Vector3));
    Vector3 size = r.ReadVec3();
    ASSERT_NEAR(size.x, 4.0f, 0.001f);

    // Third property: Anchored
    std::string name2 = r.ReadString();
    ASSERT_EQ(name2, "Anchored");
    uint8_t kind2 = r.ReadU8();
    ASSERT_EQ(kind2, static_cast<uint8_t>(PropertyValue::Kind::Bool));
    ASSERT_EQ(r.ReadU8(), 1);

    PASS();
}

// ===== CreateObject with embedded properties test =====

TEST(create_object_with_properties) {
    PacketWriter writer;
    NetworkID id = 100;
    NetworkID parentID = 2;

    writer.WriteU8(static_cast<uint8_t>(PacketType::CreateObject));
    writer.WriteU32(id);
    writer.WriteU32(parentID);
    writer.WriteString("Part");
    writer.WriteString("TestPart");

    // Embedded properties
    writer.WriteU16(2); // 2 properties
    // CFrame
    writer.WriteString("CFrame");
    writer.WriteU8(static_cast<uint8_t>(PropertyValue::Kind::CFrame));
    writer.WriteVec3({5, 10, 15});
    // Identity rotation matrix (column-major: [col][row])
    glm::mat3 rot(1.0f);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            writer.WriteFloat(rot[i][j]);
    // Anchored
    writer.WriteString("Anchored");
    writer.WriteU8(static_cast<uint8_t>(PropertyValue::Kind::Bool));
    writer.WriteU8(0);

    // Parse it back
    PacketReader r(writer.GetData().data(), writer.GetData().size());
    auto type = static_cast<PacketType>(r.ReadU8());
    ASSERT_EQ(type, PacketType::CreateObject);
    ASSERT_EQ(r.ReadU32(), 100u);
    ASSERT_EQ(r.ReadU32(), 2u);
    ASSERT_EQ(r.ReadString(), "Part");
    ASSERT_EQ(r.ReadString(), "TestPart");

    uint16_t propCount = r.ReadU16();
    ASSERT_EQ(propCount, 2);

    // CFrame property
    ASSERT_EQ(r.ReadString(), "CFrame");
    uint8_t cfKind = r.ReadU8();
    ASSERT_EQ(cfKind, static_cast<uint8_t>(PropertyValue::Kind::CFrame));
    Vector3 pos = r.ReadVec3();
    ASSERT_NEAR(pos.x, 5.0f, 0.001f);
    ASSERT_NEAR(pos.y, 10.0f, 0.001f);
    ASSERT_NEAR(pos.z, 15.0f, 0.001f);
    // Skip 9 rotation floats
    for (int i = 0; i < 9; i++) r.ReadFloat();

    // Anchored property
    std::string propName = r.ReadString();
    ASSERT_EQ(propName, "Anchored");
    uint8_t ancKind = r.ReadU8();
    ASSERT_EQ(ancKind, static_cast<uint8_t>(PropertyValue::Kind::Bool));
    ASSERT_EQ(r.ReadU8(), 0);

    ASSERT_TRUE(!r.HasMore());
    PASS();
}

// ===== Packet size sanity test =====

TEST(create_object_packet_size_reasonable) {
    // A CreateObject with 6 properties should be under 500 bytes
    PacketWriter writer;
    writer.WriteU8(static_cast<uint8_t>(PacketType::CreateObject));
    writer.WriteU32(12345);
    writer.WriteU32(2);
    writer.WriteString("Part");
    writer.WriteString("SomePart");
    writer.WriteU16(6);

    // CFrame (48 bytes value + 1 kind + 6 name)
    writer.WriteString("CFrame");
    writer.WriteU8(static_cast<uint8_t>(PropertyValue::Kind::CFrame));
    writer.WriteVec3({0, 0, 0});
    for (int i = 0; i < 9; i++) writer.WriteFloat(0);

    // 5 more simple properties
    for (auto& pname : {"Size", "Anchored", "CanCollide", "Transparency", "BrickColor"}) {
        writer.WriteString(pname);
        if (strcmp(pname, "Size") == 0) {
            writer.WriteU8(static_cast<uint8_t>(PropertyValue::Kind::Vector3));
            writer.WriteVec3({4, 1.2f, 2});
        } else if (strcmp(pname, "Transparency") == 0) {
            writer.WriteU8(static_cast<uint8_t>(PropertyValue::Kind::Float));
            writer.WriteFloat(0.0f);
        } else if (strcmp(pname, "BrickColor") == 0) {
            writer.WriteU8(static_cast<uint8_t>(PropertyValue::Kind::Int));
            writer.WriteU32(194);
        } else {
            writer.WriteU8(static_cast<uint8_t>(PropertyValue::Kind::Bool));
            writer.WriteU8(1);
        }
    }

    ASSERT_TRUE(writer.Size() < 500);
    printf("(size=%zu) ", writer.Size());
    PASS();
}

// ===== Main =====

int main() {
    printf("\n=== Nova Replication System Tests ===\n\n");
    // Tests are auto-registered via static initialization
    printf("\nResults: %d passed, %d failed\n\n", testsPassed, testsFailed);
    return testsFailed > 0 ? 1 : 0;
}
