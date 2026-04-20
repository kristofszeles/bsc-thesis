import java.io.File

plugins {
    id("com.android.application")
}

android {
    namespace = "org.libsdl.app"
    compileSdk = 34
    ndkVersion = "26.1.10909125"

    defaultConfig {
        applicationId = "com.bscthesis.maze_game"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments += "-DANDROID_STL=c++_shared"
                arguments += "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
        }
        debug {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    externalNativeBuild {
        cmake {
            path = file("jni/CMakeLists.txt")
        }
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
}

val syncMazeAssets by tasks.registering(Copy::class) {
    val maze = rootProject.projectDir.resolve("../maze-game")
    into(layout.projectDirectory.dir("src/main/assets"))
    from(maze.resolve("textures")) { into("textures") }
    from(maze.resolve("fonts")) { into("fonts") }
    from(maze.resolve("models")) { into("models") }
}

val syncLauncherIcon by tasks.registering(Copy::class) {
    from(rootProject.projectDir.resolve("../maze-game/textures/bg.png"))
    into(layout.projectDirectory.dir("src/main/res/drawable"))
    rename { "ic_launcher.png" }
}

val generateAndroidAssetFileLists by tasks.registering {
    doLast {
        val mazeDir = rootProject.projectDir.resolve("../maze-game").canonicalFile
        val outDir = layout.projectDirectory.dir("src/main/assets/filelists").get().asFile
        outDir.mkdirs()
        val rootPath = mazeDir.toPath()
        fun writeList(listName: String, dir: File) {
            check(dir.isDirectory) { "Expected directory: ${dir.absolutePath}" }
            val files = dir.listFiles()?.filter { it.isFile }?.sortedBy { it.name } ?: emptyList()
            val text = files.joinToString("\n") { f ->
                rootPath.relativize(f.toPath()).toString().replace('\\', '/')
            } + "\n"
            File(outDir, listName).writeText(text)
        }
        writeList("textures_root.txt", File(mazeDir, "textures"))
        writeList("skyboxes.txt", File(mazeDir, "textures/skyboxes"))
        writeList("tiles.txt", File(mazeDir, "textures/tiles"))
        writeList("models_root.txt", File(mazeDir, "models"))
        writeList("vehicles.txt", File(mazeDir, "models/vehicles"))
    }
}

tasks.named("preBuild") {
    dependsOn(syncMazeAssets, syncLauncherIcon, generateAndroidAssetFileLists)
}

dependencies {}
