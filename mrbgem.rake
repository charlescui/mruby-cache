MRuby::Gem::Specification.new('mruby-cache') do |spec|
  spec.license = 'MIT'
  spec.authors = 'Charles Cui'
  spec.linker.libraries << ['pthread', 'rt']
end
