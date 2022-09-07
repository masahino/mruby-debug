MRuby::Gem::Specification.new('mruby-debug') do |spec|
  
  spec.license = 'MIT'
  spec.authors = 'masahino'

  spec.build.defines |= %w(MRB_ENABLE_DEBUG_HOOK)
end
