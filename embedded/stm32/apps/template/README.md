# Template Application

## App Struture

This application is a "Hello World" application for ENTS.

```
.
├── main.c
├── Makefile
└── README.md
```

`main.c`: Code directly used in the application.
`Makefile`: Makefile for building the application. By defaults it builds all `*.c` files in the directory. NOTE: You cannot have sources in folder for apps.
`README.md`: This file. Include description and any special instructions for the application.

## Building

To build:

```bash
make
```

To upload to hardware:

```bash
make install
```
