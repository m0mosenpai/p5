#! /bin/env python

import toolspath
from testing import Xv6Build, Xv6Test


class test1(Xv6Test):
    name = "test_26"
    description = "MAP PLACEMENT: Check wmap, wunmap and then wmap again works Place 3 maps, delete map1 and map3, place 2 more maps"
    tester = "ctests/test_26.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 3
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"


class test2(Xv6Test):
    name = "test_27"
    description = "Fork: New map in child that is not present in parent, Same   map contents exist in both parent and childs"
    "Checks the presence of Segmentation Fault"
    tester = "ctests/test_27.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 3
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"
    timeout = 120

class test3(Xv6Test):
    name = "test_28"
    description = " COW: 2 layers of forking: allocated array has diff pa in parent and child after modification"
    tester = "ctests/test_28.c"
    header = "ctests/tester.h"
    make_qemu_args = "CPUS=1"
    point_value = 4
    success_pattern = "PASSED"
    failure_pattern = "Segmentation Fault"
    timeout = 60


from testing.runtests import main

main(
    Xv6Build,
    all_tests=[
        test1,
        test2,
        test3,
    ],
    # Add your test groups here
    # End of test groups
)
