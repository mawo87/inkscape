# SPDX-License-Identifier: GPL-2.0-or-later

# Tests to run for 64-bit builds only. These fail in 32-bit builds (possibly due to rounding issues)
# TODO: Figure out actual cause and see if we can fix it
if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(RENDERING_TESTS_64bit
        test-rtl-vertical

        # .otf font with compressed SVG glyphs
        text-gzipped-svg-glyph
    )
endif()

#add your test here (do not put the .svg extension)
set(RENDERING_TESTS
    # -- Generic tests --
    test-empty
    test-dont-crash
    test-use

    # -- Selector tests --
    selector-important-002
    selector-important-003

    multi-style
    style-parsing

    # -- Text tests --
    ## Many (if not all) of these tests are sensitive to the text rendering stack: FreeType, HarfBuzz, Pango.

    # test-baseline-shift
    ## Small differences with code adapted for Pango 1.44.
    
    test-glyph-y-pos 
    ## to be fixed since an update happened between harfbuzz 1.5.1(OK) and 1.6.0(FAIL).
    ## If you re-enable the test, you may have to *slightly* fix the expected rendering (hoping the fix happens upstream). 
    ## Please also check that the rendering with harfbuzz <=1.5.1 is not *too* wrong (for older systems)
    ## cf Tav's post : https://www.patreon.com/posts/into-sinkhole-19021727
    ## and bug https://bugzilla.gnome.org/show_bug.cgi?id=787526

    # text-shaping
    ## Expected rendering generated with Pango 1.44. Currently fails with
    ## CI as CI uses Pango 1.40. Enable after updating CI to Ubuntu 20.04.

    text-glyphs-combining
    ## Expected rendering generated with Pango 1.44.

    text-glyphs-vertical
    ## Expected rendering generated with Pango 1.44.
    
    # -- LPE tests --
    test-powerstroke-join

    # <symbol> geometric properties (SVG 2.0 feature)
    symbol-svg2-geometry-properties
    symbol-svg2-ref

    ${RENDERING_TESTS_64bit}
)


foreach(rendering_test ${RENDERING_TESTS})
    set(testname "render_${rendering_test}")
    add_test(NAME ${testname}
             COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/inkscape ${CMAKE_CURRENT_SOURCE_DIR}/${rendering_test}
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/testfiles/rendering_tests)
    set_tests_properties(${testname} PROPERTIES ENVIRONMENT "${INKSCAPE_TEST_PROFILE_DIR_ENV}/${testname};${CMAKE_CTEST_ENV}")
endforeach()

