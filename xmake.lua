-- include subprojects
includes("lib/commonlibf4")

-- set project constants
set_project("ThirdPersonCrosshair")
set_version("0.1.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- define targets
target("ThirdPersonCrosshair")
    add_rules("commonlibf4.plugin", {
        name = "ThirdPersonCrosshair",
        author = "ValenteJesusLR",
        description = "Draws a real crosshair instead of the dot when aiming in third person"
    })

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

    -- ship the ini next to the dll
    add_installfiles("dist/(F4SE/Plugins/*.ini)")
