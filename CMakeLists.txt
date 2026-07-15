# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(TIME_LOOP)
# สำหรับเวอร์ชันล่าสุด ให้แปะโค้ดชุดนี้ไว้ที่ท้ายไฟล์ตัวนอกสุดแทนครับ

# บังคับลบค่าล้างกฎ -Werror ทิ้งไปจากตัวแปรส่วนกลางของระบบทั้งหมด
string(REPLACE "-Werror=all" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REPLACE "-Werror=all" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "-Werror" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# บังคับป้อนคำสั่งเพิกเฉยต่อการแจ้งเตือนพารามิเตอร์ของบอร์ดแบบเจาะจงลงไปใน Base Flags 
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=missing-field-initializers -Wno-error")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=missing-field-initializers -Wno-error")