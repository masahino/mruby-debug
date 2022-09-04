# mruby-debug   [![build](https://github.com/masahino/mruby-debug/actions/workflows/ci.yml/badge.svg)](https://github.com/masahino/mruby-debug/actions/workflows/ci.yml)
Debug class
## install by mrbgems
- add conf.gem line to `build_config.rb`

```ruby
MRuby::Build.new do |conf|

    # ... (snip) ...

    conf.gem :github => 'masahino/mruby-debug'
end
```
## example
```ruby
p Debug.hi
#=> "hi!!"
t = Debug.new "hello"
p t.hello
#=> "hello"
p t.bye
#=> "hello bye"
```

## License
under the MIT License:
- see LICENSE file
