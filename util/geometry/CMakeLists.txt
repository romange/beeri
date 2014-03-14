add_definitions(-D S2_USE_EXACTFLOAT)
add_library(s2 s1angle.cc
            s1interval.cc
            s2cap.cc
            s2.cc
            s2cell.cc
            s2cellid.cc
            s2cellunion.cc
            s2edgeindex.cc
            s2edgeutil.cc
            s2latlng.cc
            s2latlngrect.cc
            s2loop.cc
            s2pointregion.cc
            s2polygonbuilder.cc
            s2polygon.cc
            s2polyline.cc
            s2r2rect.cc
            s2region.cc
            s2regioncoverer.cc
            s2regionintersection.cc
            s2regionunion.cc)

cxx_link(s2 math strings coding)

cxx_test(s2cap_test s2)

add_library(s2_testing s2testing.cc)
cxx_link(s2_testing s2)

cxx_test(s2polygonbuilder_test s2_testing)
cxx_test(s2cellid_test s2_testing)
