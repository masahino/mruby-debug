MRuby::Gem::Specification.new('mruby-debug') do |spec|
  spec.license = 'MIT'
  spec.authors = 'masahino'

  spec.build.defines << "MRB_USE_DEBUG_HOOK"
end
