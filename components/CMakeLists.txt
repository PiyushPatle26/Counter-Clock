cmake_minimum_required(VERSION 3.16)

set(srcs "esp-idf-ds1307/main/ds1307.c"
         "esp-idf-ds1307/main/i2cdev.c"
         "esp-idf-pca9685/src/pca9685.c")

set(includes "esp-idf-ds1307/main"
             "esp-idf-pca9685/src"
             "esp-idf-pca9685/include")

idf_component_register(SRCS ${srcs}
                      INCLUDE_DIRS ${includes}
                      REQUIRES driver)  