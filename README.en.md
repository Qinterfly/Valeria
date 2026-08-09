[![ru](https://img.shields.io/badge/lang-ru-red.svg)](https://github.com/Qinterfly/Modus/blob/master/README.md)
[![en](https://img.shields.io/badge/lang-en-green.svg)](README.en.md)
[![pdf](https://img.shields.io/badge/example-PDF-blue)](help/output.pdf)

`Valeria` is designed to generate reports based on ground vibration test results. To obtain recorded responses, the application interacts with the `LMS TestLab Automation` API.

![GUI](help/images/full.png)

Users can define page layouts by creating graphical elements of the following types:
* text
* plot
* table
* envelope
* image

After creating elements, their properties can be edited and measurement points can be assigned to collect data. The application also supports macros that can be used in any field of these elements. The resulting page layout can be saved as a `JSON` file and reused later for this structure or for similar layouts.

In addition, if data were recorded using a measurement system incompatible with `LMS TestLab`, the data can be loaded through the response editor. The response editor uses tabs linked to each signal for easy navigation.

![Response set editor](help/images/response-editor.png)

[Sample report](help/output.pdf)
