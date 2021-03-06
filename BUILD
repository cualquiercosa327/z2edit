package(default_visibility = ["//visibility:public"])

load("@mxebzl//tools:rules.bzl", "pkg_winzip")

config_setting(
    name = "windows",
    values = {
        "crosstool_top": "@mxebzl//compiler:win64",
    },
)

genrule(
    name = "make_version",
    outs = ["version.h"],
    cmd = """sed -e 's/ / "/g' -e 's/$$/"\\n/g' -e "s/^/#define /g" bazel-out/volatile-status.txt > $@""",
    stamp = 1,
)

cc_library(
    name = "app",
    srcs = [
        "app.cc",
    ],
    hdrs = [
        "app.h",
        "version.h",
    ],
    linkopts = [
        "-lSDL2_image",
        "-lSDL2_mixer",
        "-lSDL2_gfx",
        "-lSDL2",
    ],
    deps = [
        "//imwidget:base",
        "//imwidget:drops",
        "//imwidget:editor",
        "//imwidget:enemyattr",
        "//imwidget:hwpalette",
        "//imwidget:misc_hacks",
        "//imwidget:map_connect",
        "//imwidget:neschrview",
        "//imwidget:palace_gfx",
        "//imwidget:palette",
        "//imwidget:project",
        "//imwidget:rom_memory",
        "//imwidget:simplemap",
        "//imwidget:start_values",
        "//imwidget:text_table",
        "//imwidget:tile_transform",
        "//imwidget:item_effects",
        "//imwidget:object_table",
        "//imwidget:xptable",
        "//nes:cartridge",
        "//nes:chr_util",
        "//nes:cpu6502",
        "//nes:mappers",
        "//nes:text_encoding",
        "//proto:rominfo",
        "//util:browser",
        "//util:fpsmgr",
        "//util:imgui_sdl_opengl",
        "//util:os",
        "//util:logging",
        "//external:gflags",

        # TODO(cfrantz): on ubuntu 16 with MIR, there is a library conflict
        # between MIR (linked with protobuf 2.6.1) and this program,
        # which builds with protbuf 3.x.x.  A temporary workaround is to
        # not link with nfd (native-file-dialog).
        "//external:nfd",
    ],
)

filegroup(
    name = "content",
    srcs = glob(["content/*.textpb"]),
)

genrule(
    name = "make_zelda2_config",
    srcs = [
        "zelda2.textpb",
        ":content",
    ],
    outs = ["zelda2_config.h"],
    cmd = "$(location //tools:pack_config) --config $(location zelda2.textpb)" +
          " --symbol kZelda2Cfg > $(@)",
    tools = ["//tools:pack_config"],
)

cc_binary(
    name = "z2edit",
    srcs = [
        "main.cc",
        "zelda2_config.h",
    ],
    linkopts = select({
        ":windows": [
            "-lpthread",
            "-lm",
            "-lopengl32",
            "-ldinput8",
            "-ldxguid",
            "-ldxerr8",
            "-luser32",
            "-lgdi32",
            "-lwinmm",
            "-limm32",
            "-lole32",
            "-loleaut32",
            "-lshell32",
            "-lversion",
            "-luuid",
            "-lmingw32",
            "-lSDL2main",
        ],
        "//conditions:default": [
            "-lpthread",
            "-lm",
            "-lGL",
        ],
    }),
    deps = [
        ":app",
        "//external:gflags",
        "//util:config",
    ],
)

pkg_winzip(
    name = "z2edit-windows",
    files = [
        ":z2edit",
    ],
)
