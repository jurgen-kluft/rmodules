package rdno_modules

import (
	denv "github.com/jurgen-kluft/ccode/denv"
	rdno_core "github.com/jurgen-kluft/rdno_core/package"
)

const (
	repo_path = "github.com\\jurgen-kluft"
	repo_name = "rdno_modules"
)

// rdno_modules is a  package for Arduino projects that holds a
// couple of sensor objects, namely:
// - BME280 (temperature, humidity, pressure)
// - BH1750 (light sensor)
// - Sensirion/SCD41 (carbon dioxide sensor, temperature, humidity)
func GetPackage() *denv.Package {
	// dependencies
	corepkg := rdno_core.GetPackage()

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
