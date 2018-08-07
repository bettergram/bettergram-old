# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

{
  'conditions': [[ 'build_macstore', {
    'xcode_settings': {
      'SKIP_INSTALL': 'YES',
    }
  }]],
  'includes': [
    '../common.gypi',
  ],
  'variables': {
    'libs_loc': '../../../../Libraries',
    'src_loc': '../../SourceFiles',
    'submodules_loc': '../../ThirdParty',
    'mac_target': '10.10',
    'list_tests_command': 'python <(DEPTH)/tests/list_tests.py --input <(DEPTH)/tests/tests_list.txt',
  },
  'targets': [{
    'target_name': 'tests',
    'type': 'none',
    'includes': [
      '../common.gypi',
    ],
    'dependencies': [
      '<!@(<(list_tests_command))',
    ],
    'sources': [
      '<!@(<(list_tests_command) --sources)',
    ],
    'rules': [{
      'rule_name': 'run_tests',
      'extension': 'test',
      'inputs': [
        '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT)<(exe_ext)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).timestamp',
      ],
      'action': [
        '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT)<(exe_ext)',
        '--touch', '<(SHARED_INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).timestamp',
      ],
      'message': 'Running <(RULE_INPUT_ROOT)..',
    }]
  }, {
    'target_name': 'tests_algorithm',
    'includes': [
      'common_test.gypi',
    ],
    'sources': [
      '<(src_loc)/base/algorithm.h',
      '<(src_loc)/base/algorithm_tests.cpp',
    ],
  }, {
    'target_name': 'tests_flags',
    'includes': [
      'common_test.gypi',
    ],
    'sources': [
      '<(src_loc)/base/flags.h',
      '<(src_loc)/base/flags_tests.cpp',
    ],
  }, {
    'target_name': 'tests_flat_map',
    'includes': [
      'common_test.gypi',
    ],
    'sources': [
      '<(src_loc)/base/flat_map.h',
      '<(src_loc)/base/flat_map_tests.cpp',
    ],
  }, {
    'target_name': 'tests_flat_set',
    'includes': [
      'common_test.gypi',
    ],
    'sources': [
      '<(src_loc)/base/flat_set.h',
      '<(src_loc)/base/flat_set_tests.cpp',
    ],
  }, {
    'target_name': 'tests_rpl',
    'includes': [
      'common_test.gypi',
    ],
    'sources': [
      '<(src_loc)/rpl/details/callable.h',
      '<(src_loc)/rpl/details/superset_type.h',
      '<(src_loc)/rpl/details/type_list.h',
      '<(src_loc)/rpl/after_next.h',
      '<(src_loc)/rpl/before_next.h',
      '<(src_loc)/rpl/combine.h',
      '<(src_loc)/rpl/combine_previous.h',
      '<(src_loc)/rpl/complete.h',
      '<(src_loc)/rpl/consumer.h',
      '<(src_loc)/rpl/deferred.h',
      '<(src_loc)/rpl/distinct_until_changed.h',
      '<(src_loc)/rpl/event_stream.h',
      '<(src_loc)/rpl/fail.h',
      '<(src_loc)/rpl/filter.h',
      '<(src_loc)/rpl/flatten_latest.h',
      '<(src_loc)/rpl/lifetime.h',
      '<(src_loc)/rpl/map.h',
      '<(src_loc)/rpl/mappers.h',
	  '<(src_loc)/rpl/merge.h',
      '<(src_loc)/rpl/never.h',
      '<(src_loc)/rpl/operators_tests.cpp',
      '<(src_loc)/rpl/producer.h',
      '<(src_loc)/rpl/producer_tests.cpp',
      '<(src_loc)/rpl/range.h',
      '<(src_loc)/rpl/rpl.h',
	  '<(src_loc)/rpl/skip.h',
      '<(src_loc)/rpl/take.h',
      '<(src_loc)/rpl/then.h',
      '<(src_loc)/rpl/type_erased.h',
      '<(src_loc)/rpl/variable.h',
      '<(src_loc)/rpl/variable_tests.cpp',
    ],
  }],
}
