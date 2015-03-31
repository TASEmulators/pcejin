# Format #

MC2 is ASCII plain text. It consists of several key-value pairs followed by an inputlog section.

The inputlog section can be identified by its starting with a | (pipe). The inputlog section terminates at eof. Newlines may be \r\n or \n

Key-value pairs consist of a key identifier, followed by a space separator, followed by the value text. Value text is always terminated by a newline, which the value text will not include. The value text is parsed differently depending on the type of the key. The key-value pairs may be in any order, except that the first key must be version.

Integer keys (also used for Booleans, with a 1 or 0) will have a value that is a simple integer not to exceed 32bits

  * version (required) - the version of the movie file format; for now it is always 1
  * emuVersion (required) - the version of the emulator used to produce the movie
  * rerecordCount (optional) - the rerecord count
  * PCECD (bool) (optional) - true if the movie was recording on a CD game
  * ports (note C) - indicates the number of control inputs (valid ranges are 1-5)

  * comment (optional) - simply a memo.
  * by convention, the first token in the comment value is the subject of the comment.
  * by convention, subsequent comments with the same subject will have their ordering preserved and may be used to approximate multiline comments.
  * by convention, the author of the movie should be stored in comment(s) with a subject of: author

# Inputlog #

The inputlog section consists of lines beginning and ending with a | (pipe). The fields are as follows, except as noted in note C.
c 	port0 	port1 	port2

Field c is a variable length decimal integer which is a bitfield corresponding to miscellaneous input states which are valid at the start of the frame. Current values for this are

  * MOVIECMD\_RESET = 1

The format of port0, port1, port2 depends on which types of devices were attached.
SI\_GAMEPAD

  * the field consists of eight characters which constitute a bitfield.
  * any character other than ' ' or '.' means that the button was pressed. By convention, the following mnemonics will be used in a column to remind us of which button corresponds to which column:
  * UDLR12NS (Up,Down,Left,Right,I,II,Run,Select)

# Emulator Framerate constant #

  * FPS = 7159090.90909090 / 455 / 263;  (approx 59.826 fps)