luaowt
######

1-wire temperature readout and publisher written in lua. Also see owt_ and owtweb_.

.. _owt: https://bitbucket.org/blueluna/owt
.. _owtweb: https://bitbucket.org/blueluna/owtweb

(c) 2011 Erik Svensson <erik.public@gmail.com>
Licensed under the MIT license.

Requirements
------------

 * Lua_
 * LPEG_
 * luasocket_
 * luajson_
 * Linux 1-wire master and 1-wire thermal slave(s)

.. _lua: http://www.lua.org/
.. _LPEG: http://www.inf.puc-rio.br/~roberto/lpeg/lpeg.html
.. _luasocket: http://luasocket.luaforge.net
.. _luajson: https://github.com/harningt/luajson

Setup
-----

This code is meant for a linux kernel with support for a 1-wire master and the 1-wire thermal drivers.
This has been tested on Linux 3.1.4 on a Atmel AT91SAM9260 (ARM).

Use ``make`` to build the c extensions.
..
    
    $ make

This will result in ``ks0066.so`` and ``sysinfo.so``.

run ``luaowt.lua``.
..
    
    $ lua luaowt.lua
