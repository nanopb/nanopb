// swift-tools-version:5.3
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
  name: "nanopb",
  products: [
    .library(
      name: "nanopb",
      targets: ["nanopb"]
    )
  ],

  targets: [
    .target(
      name: "nanopb",
      path: ".",
      sources: [
        "src",
        "include/nanopb"
      ],
      resources: [.process("spm_resources/PrivacyInfo.xcprivacy")],
      publicHeadersPath: "include",
      cSettings: [
        .define("PB_FIELD_32BIT", to: "1"),
        .define("PB_NO_PACKED_STRUCTS", to: "1"),
        .define("PB_ENABLE_MALLOC", to: "1"),
      ]
    ),
    .testTarget(
      name: "swift-test",
      dependencies: [
        "nanopb",
      ],
      path: "build-tests/spm-swift",
      cSettings: [
        .headerSearchPath("include/nanopb"),
        .define("PB_FIELD_32BIT", to: "1"),
        .define("PB_NO_PACKED_STRUCTS", to: "1"),
        .define("PB_ENABLE_MALLOC", to: "1"),
      ]
    ),
    .testTarget(
      name: "objc-test",
      dependencies: [
        "nanopb",
      ],
      path: "build-tests/spm-objc",
      cSettings: [
        .headerSearchPath("include/nanopb"),
        .define("PB_FIELD_32BIT", to: "1"),
        .define("PB_NO_PACKED_STRUCTS", to: "1"),
        .define("PB_ENABLE_MALLOC", to: "1"),
      ]
    )
  ]
)

