#include "GtsJsonValue.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        GtsJsonValue object = GtsJsonValue::Object{{"boolean", false},
                                                   {"zero", 0},
                                                   {"text", ""},
                                                   {"null", GtsJsonValue{}},
                                                   {"array", GtsJsonValue::Array{1, "two"}},
                                                   {"object", GtsJsonValue::Object{}}};
        require(object.findBool("boolean") == false, "False must be a present value");
        require(object.findInt32("zero") == 0 && object.findUInt32("zero") == 0, "Zero must be present");
        require(object.findString("text") == "", "Empty string must be present");
        require(object.findBool("missing").value_or(true), "Missing fallback");
        require(!object.findBool("boolean").value_or(true), "False must not use fallback");
        for (const auto key : {"missing", "null", "text", "array", "object"})
            require(!object.findBool(key), "No boolean coercion");
        require(object.find("missing") == nullptr && object.find("null")->isNull(), "Missing versus null");
        require(object.findArray("array")->size() == 2 && object.findObject("object")->empty(), "Containers");
        require(!object.findArray("text") && !object.findObject("array"), "Wrong container type");
        require(!object.at(0), "Object is not an array");
        const auto* array = object.find("array");
        require(array->at(0)->tryInt32() == 1 && array->at(1)->tryString() == "two", "Indexed access");
        require(!array->at(2) && !array->at(std::numeric_limits<size_t>::max()), "Bounds checking");
        require(!GtsJsonValue(false).find("anything") && !GtsJsonValue(false).findBool("anything"),
                "Non-object lookup");
        require(!GtsJsonValue("1").tryNumber() && !GtsJsonValue(true).tryInt32(), "No scalar coercion");

        require(GtsJsonValue(std::numeric_limits<int32_t>::min()).tryInt32() == std::numeric_limits<int32_t>::min(),
                "Signed lower boundary");
        require(GtsJsonValue(uint64_t{2147483647}).tryInt32() == std::numeric_limits<int32_t>::max(),
                "Unsigned to signed boundary");
        require(!GtsJsonValue(int64_t{-2147483649}).tryInt32() && !GtsJsonValue(uint64_t{2147483648}).tryInt32(),
                "Signed overflow");
        require(GtsJsonValue(int64_t{4294967295}).tryUInt32() == std::numeric_limits<uint32_t>::max(),
                "Signed to unsigned boundary");
        require(!GtsJsonValue(-1).tryUInt32() && !GtsJsonValue(uint64_t{4294967296}).tryUInt32(), "Unsigned range");
        require(GtsJsonValue(-2147483648.0).tryInt32() == std::numeric_limits<int32_t>::min() &&
                    GtsJsonValue(4294967295.0).tryUInt32() == std::numeric_limits<uint32_t>::max(),
                "Double boundaries");
        require(!GtsJsonValue(-2147483649.0).tryInt32() && !GtsJsonValue(4294967296.0).tryUInt32(),
                "Double integer overflow");
        for (double number : {-1.5, 0.5, 2147483647.5})
            require(!GtsJsonValue(number).tryInt32() && !GtsJsonValue(number).tryUInt32(), "Fraction rejection");
        require(GtsJsonValue(-0.0).tryUInt32() == 0, "Negative zero is integral");
        require(GtsJsonValue(uint64_t{9007199254740993}).tryUInt64() == uint64_t{9007199254740993},
                "Integer precision beyond double");
        require(GtsJsonValue(std::numeric_limits<uint64_t>::max()).tryUInt64() == std::numeric_limits<uint64_t>::max(),
                "Full uint64 range");
        require(!GtsJsonValue(-1).tryUInt64() && !GtsJsonValue(0x1p64).tryUInt64(), "UInt64 overflow");
        require(GtsJsonValue(std::nextafter(0x1p64, 0.0)).tryUInt64().has_value(), "Last in-range double");
        require(GtsJsonValue(int64_t{7}).tryUInt64() == 7, "Signed to uint64");
        require(!GtsJsonValue(7.5).tryUInt64(), "UInt64 fraction");
        require(!GtsJsonValue(std::numeric_limits<uint64_t>::max()).tryInt32() &&
                    !GtsJsonValue(std::numeric_limits<int64_t>::min()).tryUInt32(),
                "Large integers not narrowed");

        require(GtsJsonValue(std::numeric_limits<float>::max()).tryFloat() == std::numeric_limits<float>::max(),
                "Float boundary");
        require(!GtsJsonValue(double(std::numeric_limits<float>::max()) * 2).tryFloat(), "Float overflow");
        require(GtsJsonValue(0.25).tryFloat() == 0.25f && GtsJsonValue(1).tryNumber() == 1.0, "Numeric access");
        for (double number : {std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity(),
                              std::numeric_limits<double>::quiet_NaN()})
        {
            GtsJsonValue invalid(number);
            require(!invalid.tryNumber() && !invalid.tryFloat() && !invalid.tryInt32() && !invalid.tryUInt32() &&
                        !invalid.tryUInt64(),
                    "Non-finite rejection");
        }
        require(GtsJsonValue::Object{{"fraction", 1.5}, {"large", uint64_t{9007199254740993}}}.size() == 2,
                "Value construction remains available");
        object = GtsJsonValue::Object{{"fraction", 1.5}, {"large", uint64_t{9007199254740993}}};
        require(object.findNumber("fraction") == 1.5 && object.findFloat("fraction") == 1.5f &&
                    object.findUInt64("large") == uint64_t{9007199254740993},
                "Typed member conversion");
        require(!object.findInt32("fraction") && !object.findFloat("missing"), "Member conversion failure");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
