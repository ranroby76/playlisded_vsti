#!/bin/bash

# ==============================================================================
# Playlisted2 Mac Installer Script (Runs on GitHub Actions)
# ==============================================================================

# 1. SETUP VARIABLES
APP_NAME="Playlisted2"
VERSION="2.0.0"
IDENTIFIER="com.fanan.playlisted2"

# Paths to the CMake Build Artifacts (Matches build_pipeline.yml)
# Note: On Mac, CMake usually puts bundles in build/Playlisted2_artefacts/Release/...
PATH_AU="build/Playlisted2_artefacts/Release/AU"
PATH_VST3="build/Playlisted2_artefacts/Release/VST3"
PATH_CLAP="build/Playlisted2_artefacts/Release/CLAP"

# Output Directory
mkdir -p staging
rm -f "$APP_NAME_Mac_Installer.pkg"

echo "--- [1/4] Building Intermediate Component Packages ---"

# 2. BUILD COMPONENT PACKAGES (Invisible internal packages)
# ------------------------------------------------------------------------------

# AU Package
pkgbuild --root "$PATH_AU" \
         --identifier "$IDENTIFIER.au" \
         --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/Components" \
         "staging/au.pkg"

# VST3 Package
pkgbuild --root "$PATH_VST3" \
         --identifier "$IDENTIFIER.vst3" \
         --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/VST3" \
         "staging/vst3.pkg"

# CLAP Package
pkgbuild --root "$PATH_CLAP" \
         --identifier "$IDENTIFIER.clap" \
         --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/CLAP" \
         "staging/clap.pkg"

echo "--- [2/4] Generating Distribution XML ---"

# 3. GENERATE DISTRIBUTION XML (The "Clever" Menu)
# ------------------------------------------------------------------------------
# This creates the menu structure for the installer

cat <<EOF > distribution.xml
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>$APP_NAME</title>
    <welcome file="Welcome.txt" mime-type="text/plain"/>
    <license file="License File EULA.txt" mime-type="text/plain"/>
    <options customize="always" require-scripts="false"/>
    
    <choices-outline>
        <line choice="choice_vst3"/>
        <line choice="choice_au"/>
        <line choice="choice_clap"/>
    </choices-outline>

    <choice id="choice_vst3" title="VST3 Plugin" description="For Cubase, Studio One, Ableton Live, Reaper." start_selected="true">
        <pkg-ref id="$IDENTIFIER.vst3"/>
    </choice>

    <choice id="choice_au" title="Audio Unit (AU)" description="For Logic Pro, GarageBand, MainStage." start_selected="true">
        <pkg-ref id="$IDENTIFIER.au"/>
    </choice>

    <choice id="choice_clap" title="CLAP Plugin" description="For Bitwig Studio, FL Studio, Reaper." start_selected="true">
        <pkg-ref id="$IDENTIFIER.clap"/>
    </choice>

    <pkg-ref id="$IDENTIFIER.vst3" version="$VERSION" onConclusion="none">vst3.pkg</pkg-ref>
    <pkg-ref id="$IDENTIFIER.au" version="$VERSION" onConclusion="none">au.pkg</pkg-ref>
    <pkg-ref id="$IDENTIFIER.clap" version="$VERSION" onConclusion="none">clap.pkg</pkg-ref>
</installer-gui-script>
EOF

echo "--- [3/4] Preparing Resources ---"

# Create a temporary resources folder for the license
mkdir -p Resources
# Copy your EULA to the resources folder so productbuild can find it
cp "License File EULA.txt" Resources/
# Create a dummy welcome text
echo "Welcome to the Playlisted2 Installer for macOS." > Resources/Welcome.txt

echo "--- [4/4] Building Final Distribution Package ---"

# 4. BUILD FINAL PACKAGE
# ------------------------------------------------------------------------------
productbuild --distribution distribution.xml \
             --package-path staging \
             --resources Resources \
             "$APP_NAME_Mac_Installer.pkg"

echo "SUCCESS: Installer created at $APP_NAME_Mac_Installer.pkg"