set_project("Nova07")
set_version("0.1.0")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "build" })

set_languages("c++20")

-- Dependencies
add_requires("libsdl3", { configs = { vulkan = true } })
add_requires("libsdl3_image", { configs = { jpeg = true, png = true, tiff = true, webp = true } })
add_requires("shaderc")
add_requires("luau")
add_requires("luabridge3")
add_requires("glm")
add_requires("joltphysics")
add_requires("pugixml")
add_requires("tracy v0.12.2", { configs = { on_demand = true } })

-- Shader compilation rule using glslc (from shaderc)
rule("hlsl2spv")
    set_extensions(".hlsl")
    on_buildcmd_file( function (target, batchcmds, sourcefile, opt)
        import("lib.detect.find_tool")

        local glslc = find_tool("glslc")
        if not glslc then
            raise("glslc not found! Install shaderc or the Vulkan SDK.")
        end

        local basename = path.basename(sourcefile)
        local stage = nil
        if basename:find("vert") then
            stage = "vertex"
        elseif basename:find("frag") then
            stage = "fragment"
        else
            raise("Cannot determine shader stage from filename: " .. sourcefile)
        end

        local outputfile = sourcefile:gsub("%.hlsl$", ".spv")

        batchcmds:show_progress(opt.progress, "${color.build.object}compiling.hlsl %s", sourcefile)
        batchcmds:vrunv(glslc.program, {
            "-fshader-stage=" .. stage,
            "-fentry-point=main",
            "--target-env=vulkan1.0",
            "--target-spv=spv1.0",
            sourcefile,
            "-o", outputfile
        })
        batchcmds:add_depfiles(sourcefile)
        batchcmds:set_depmtime(os.mtime(outputfile))
        batchcmds:set_depcache(target:dependfile(outputfile))
    end)
rule_end()

target("Nova07")
    set_kind("binary")
    set_default(true)

    add_files("src/**.cpp")
    add_includedirs("src")

    add_rules("hlsl2spv")
    add_files("shaders/**.hlsl")

    add_defines("TRACY_ENABLE")

    add_packages(
        "libsdl3",
        "libsdl3_image",
        "shaderc",
        "luau",
        "luabridge3",
        "glm",
        "joltphysics",
        "pugixml",
        "tracy"
    )
