package rmodules

import (
	denv "github.com/jurgen-kluft/ccode/denv"
	rcore "github.com/jurgen-kluft/rcore/package"
)

const (
	repo_path = "github.com\\jurgen-kluft"
	repo_name = "rmodules"
)

// rmodules is a package for Arduino projects that holds libraries:
// - A library for the rotary encoder
func GetPackage() *denv.Package {
	// dependencies
	corepkg := rcore.GetPackage()

	// main package
	mainpkg := denv.NewPackage(repo_path, repo_name)
	mainpkg.AddPackage(corepkg)

	// A library for the rotary encoder
	rotaryEncoderLib := denv.SetupCppLibraryForArduinoEsp32(mainpkg, "lib_rotary_encoder", "rotary_encoder")
	rotaryEncoderLib.AddDependencies(corepkg.GetMainLib())

	// Example apps
	basicRotaryEncoderExampleApp := denv.SetupCppAppProjectForArduino(mainpkg, "basic_rotary_encoder", "examples/basic_rotary_encoder")
	basicRotaryEncoderExampleApp.AddDependency(rotaryEncoderLib)

	mainpkg.AddMainApp(basicRotaryEncoderExampleApp)
	mainpkg.AddLibrary(rotaryEncoderLib)
	return mainpkg
}
