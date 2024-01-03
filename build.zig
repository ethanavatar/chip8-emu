const std = @import("std");
const builtin = @import("builtin");

pub fn build_raylib(b: *std.Build) *std.Build.Step {
    const raylib_prebuild = b.addSystemCommand(&.{
        "cmake",
        "-S",
        "deps/raylib",
        "-B",
        "deps/raylib/build",
    });

    const raylib_build = b.addSystemCommand(&.{
        "cmake",
        "--build",
        "deps/raylib/build",
        "--config",
        "Release",
        "-j",
    });

    raylib_build.step.dependOn(&raylib_prebuild.step);
    return &raylib_build.step;
}

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    var is_windows = false;
    if (target.os_tag == null) {
        is_windows = builtin.os.tag == .windows;
    } else if (target.os_tag == .windows) {
        is_windows = true;
    }

    const exe = b.addExecutable(.{
        .name = "chip-8",
        .target = target,
        .optimize = optimize,
    });

    exe.addIncludePath(.{ .path = "include" });
    const c_files = .{
        "src/main.c",
        "src/canvas.c",
        "src/chip8.c",
    };
    const c_flags = .{
        "-std=c99",
        "-pedantic",
        "-Wall",
        "-Wextra",
        //"-Werror",
    };

    exe.addCSourceFiles(&c_files, &c_flags);
    exe.linkLibC();

    exe.addIncludePath(.{ .path = "deps/raylib/build/raylib/include" });
    exe.addLibraryPath(.{ .path = "deps/raylib/build/raylib/Release" });
    exe.linkSystemLibrary("raylib");
    exe.linkSystemLibrary("opengl32");

    if (is_windows) {
        exe.linkSystemLibrary("user32");
        exe.linkSystemLibrary("shell32");
        exe.linkSystemLibrary("gdi32");
        exe.linkSystemLibrary("winmm");
    }

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    const raylib_step = build_raylib(b);
    const raylib_run_step = b.step("raylib", "Build raylib");
    raylib_run_step.dependOn(raylib_step);
}
