{
  "targets": [
    {
      "target_name": "zd",
      "sources": [
        "binding.cpp",
        "../../library/zonedetect.c"
      ],
      "include_dirs": [
        "<!@(nodejs -p \"require('node-addon-api').include\")",
        "../../library/"
      ],
      "dependencies": [
        "<!(nodejs -p \"require('node-addon-api').gyp\")"
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "defines": ["NAPI_CPP_EXCEPTIONS"]
    }
  ]
}
