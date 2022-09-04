# mruby-debug   [![build](https://github.com/masahino/mruby-debug/actions/workflows/ci.yml/badge.svg)](https://github.com/masahino/mruby-debug/actions/workflows/ci.yml)
Debug class
## install by mrbgems
- add conf.gem line to `build_config.rb`

```ruby
MRuby::Build.new do |conf|

    # ... (snip) ...

    conf.enable_debug
    conf.cc.defines = %w(MRB_ENABLE_DEBUG_HOOK)
    conf.gem :github => 'masahino/mruby-debug'
end

## License
under the MIT License:
- see LICENSE file
