const std = @import("std");

const VERSION: std.SemanticVersion = .{
    .major = 0,
    .minor = 4,
    .patch = 8,
};

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // On Windows, given there is no standard lib install dir etc., we rather
    // by default build static lib.
    const build_shared = b.option(
        bool,
        "md4c-shared",
        "Build md4c as a shared library",
    ) orelse !target.result.isMinGW();

    // build options
    var with_utf8 = b.option(bool, "utf8", "Use UTF8") orelse false;
    const with_utf16 = b.option(bool, "utf16", "Use UTF16") orelse false;
    const with_ascii = b.option(bool, "ascii", "Use UTF8") orelse false;

    // defaults to UTF8 if nothing else set
    if (!with_utf8 and !with_utf16 and !with_ascii) {
        with_utf8 = true;
    }

    const lib = b.addLibrary(.{
        .name = "md4c",
        .version = VERSION,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
        .linkage = if (build_shared) .dynamic else .static,
    });

    lib.addCSourceFiles(.{
        .files = &md4c_sources,
        .flags = &md4c_flags,
    });

    setDefines(lib, with_utf8, with_utf16, with_ascii);
    lib.linkLibC();
    lib.installHeader(b.path("src/md4c.h"), "md4c.h");
    b.installArtifact(lib);

    const exe = b.addExecutable(.{
        .name = "md2html",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });

    setDefines(exe, with_utf8, with_utf16, with_ascii);
    exe.addCSourceFiles(.{
        .files = &md2html_sources,
        .flags = &md4c_flags,
    });
    exe.linkLibC();

    const install_exe = b.addInstallArtifact(exe, .{});
    const md2html_step = b.step("md2html", "Compile the md2html executable");
    md2html_step.dependOn(&install_exe.step);
}

fn setDefines(
    lib_or_exe: *std.Build.Step.Compile,
    with_utf8: bool,
    with_utf16: bool,
    with_ascii: bool,
) void {
    lib_or_exe.root_module.addCMacro(
        "MD_VERSION_MAJOR",
        std.fmt.comptimePrint("{d}", .{VERSION.major}),
    );
    lib_or_exe.root_module.addCMacro(
        "MD_VERSION_MINOR",
        std.fmt.comptimePrint("{d}", .{VERSION.minor}),
    );
    lib_or_exe.root_module.addCMacro(
        "MD_VERSION_RELEASE",
        std.fmt.comptimePrint("{d}", .{VERSION.patch}),
    );

    if (with_utf8) {
        lib_or_exe.root_module.addCMacro("MD4C_USE_UTF8", "1");
    } else if (with_ascii) {
        lib_or_exe.root_module.addCMacro("MD4C_USE_ASCII", "1");
    } else if (with_utf16) {
        lib_or_exe.root_module.addCMacro("MD4C_USE_UTF16", "1");
    }
}

const md4c_flags = [_][]const u8{
    "-Wall",
};

const md4c_sources = [_][]const u8{
    "src/md4c.c",
};

const md2html_sources = [_][]const u8{
    "src/md4c.c",
    "src/md4c-html.c",
    "src/entity.c",
    "md2html/cmdline.c",
    "md2html/md2html.c",
};
