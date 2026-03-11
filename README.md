# SafeCppRename

SafeCppRename is an Unreal Engine editor plugin that allows developers to safely rename C++ classes while automatically maintaining CoreRedirects.

## Features

- Safe C++ class renaming
- Automatic CoreRedirect generation
- Redirect chain flattening
- Rename history tracking

## Example

Rename:

Q1 → Q2 → Q3

Generated redirects:

Q1 → Q3  
Q2 → Q3

## Installation

1. Clone the repository
2. Copy the plugin into your project's Plugins folder
3. Regenerate project files
4. Build the project

## Notes

Blueprint child icons may temporarily appear incorrect until the Blueprint is opened or compiled once. This is an Unreal Editor refresh behavior.