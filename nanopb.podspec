Pod::Spec.new do |s|
  s.name         = "nanopb"
  # CocoaPods minor version is minor * 10,000 + patch * 100 + fourth
  s.version      = "3.30910.0"
  s.summary      = "Protocol buffers with small code size."

  s.description  = <<-DESC
                    Nanopb is a small code-size Protocol Buffers implementation
                    in ansi C. It is especially suitable for use in
                    microcontrollers, but fits any memory restricted system.
                   DESC

  s.homepage     = "https://github.com/nanopb/nanopb"
  s.license      = { :type => 'zlib', :file => 'LICENSE.txt' }
  s.author       = { "Petteri Aimonen" => "jpa@nanopb.mail.kapsi.fi" }
  s.source       = { :git => "https://github.com/nanopb/nanopb.git", :tag => "0.3.9.10" }

  s.ios.deployment_target = '12.0'
  s.osx.deployment_target = '10.15'
  s.tvos.deployment_target = '13.0'
  s.watchos.deployment_target = '7.0'

  s.requires_arc = false
  s.xcconfig = { 'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) PB_FIELD_32BIT=1 PB_NO_PACKED_STRUCTS=1 PB_ENABLE_MALLOC=1' }

  s.source_files  = '*.{h,c}'
  s.public_header_files  = '*.h'

  s.subspec 'encode' do |e|
    e.public_header_files = ['pb.h', 'pb_encode.h', 'pb_common.h']
    e.source_files = ['pb.h', 'pb_common.h', 'pb_common.c', 'pb_encode.h', 'pb_encode.c']
  end

  s.subspec 'decode' do |d|
    d.public_header_files = ['pb.h', 'pb_decode.h', 'pb_common.h']
    d.source_files = ['pb.h', 'pb_common.h', 'pb_common.c', 'pb_decode.h', 'pb_decode.c']
  end

  s.resource_bundles = {
    "#{s.module_name}_Privacy" => 'spm_resources/PrivacyInfo.xcprivacy'
  }
end
