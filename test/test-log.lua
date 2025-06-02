local log = require('log')

do --- test basic
  local logger = log.init(0)

  logger:warn("warn")
  logger:debug("debug")
  logger:info("info")
  logger:error("error")

  logger:close()
  local ok, msg = pcall(function()
    logger:warn("warn")
  end)
  assert(ok == false)
  assert(msg and msg:find('closed'), msg)
  print('teet basic ok')
end

do --- test set
  local logger = log.init(0)
  print("level:", logger:level())
  logger:debug("debug")
  logger:level("INFO")
  assert(logger:level() == "INFO")
  print("date_fmt:", logger:date_fmt())
  logger:date_fmt('%d')
  print("time_fmt:", logger:time_fmt())
  logger:time_fmt('%S')
  print("fmt:", logger:fmt())
  logger:fmt('%d %t %l: %m\n')
  logger:warn("warn")
  print('tset set ok')
end

do --- test file
  local logger = log.init(1, "rotate.log")
  logger:level("INFO")
  logger:debug("debug")
  logger:info("INFO")
  local f = assert(io.popen("cat rotate.log"))
  local ctx = f:read("*a")
  print(ctx)
  f:close()
  logger:rotate("rotate.log")
  f = assert(io.popen("cat rotate.log.old"))
  local ctx1 = f:read("*a")
  assert(ctx == ctx1)
  f:close()
  f = assert(io.popen("cat rotate.log"))
  ctx = f:read("*a")
  assert(#ctx == 0)
  f:close()
  logger:info("after rotate")
  f = assert(io.popen("cat rotate.log"))
  ctx = f:read("*a")
  assert(#ctx > 0)
  f:close()

  logger:close()
  print('test file ok')
end

do --- test status
  local a = log.init(0)
  local logger = log.init(1, "test.log")
  print(log.status())
  logger:info('done')
  a:info('done')
  print("test status ok")
end

do --- test buffer
  local logger = log.init(0)
  logger:level("DEBUG")
  local buf = [[
82026d020100020104a482024a020101048201ec308201e83082018ba0030201020210549f0066bcf1d664ec97aa2ad13809d8300c06082a811ccf5501837505003030310b300906035504061302434e310d300b060355040a0c04544553543112301006035504030c0944656d6f534d324341301e170d3231313032363031333632365a170d3232313032363031333632365a301c310d300b06035504030c0474657374310b300906035504061302434e3059301306072a8648ce3d020106082a811ccf5501822d034200041e44f184c99311fa3c16139ceeccb8fd19cca17cb0b2d31593f23e541dfa2439792833e37be3cd923392a45828b0b5944a7dafb2a88d5df1a09063c521167b3aa38198308195300b0603551d0f0404030204f0301d0603551d0e04160414f2a20cada68188df07891d428e8397f6ccbaee6330670603551d230460305e8014f9c216e2bc03a77d4c6cd7be54e7f3e39f9bf041a134a4323030310b300906035504061302434e310d300b060355040a0c04544553543112301006035504030c0944656d6f534d32434182104d3e6b3292ff3608e8ca305a4cbb9c9a300c06082a811ccf5501837505000349003046022100d85eefc9cd48730521662755d1d0d5a0eb6067d422d197efe08fcc4256b599bc022100c6252eb88911dbca3be16c8294f05b89642ccb6b8e00e96080f396dc2ea992e402010604067365637265740447304502202f0044f5895740d6587dff21c9b15e88faab23419bd155ea80a074edcb4b61a9022100e9dfdfff0c2b5b4c79a76a65b058d766eda8af7d7719de11b72c5b1c55f38dbf020102181732303232303632343135343433302e3137302b30383030
  ]]
  logger:buffer(buf, "buffer")
  logger:info("format back")
  logger:close()
  logger = log.init(0, "test.log")
  logger:level("DEBUG")
  logger:buffer(buf, "buffer")
  logger:close()
  print('test buffer ok')
end

do --- test thread
  local _, uv = pcall(require, 'luv')
  if not _ then
    print("luv not found, skip thread test")
    return
  end
  local inspect = require('inspect')
  local function work_callback(msg)
    local _uv = require('luv')
    local _log = require('log')
    local id = tostring(_uv.thread_self())
    local logger = _log.init(2, "test.log")
    logger:level("INFO")
    logger:info(id)
    logger:info(msg)
    return id, msg
  end

  local results = {}
  local function after_work_callback(id, msg)
    results[#results + 1] = id
    results[#results + 1] = msg
  end

  local items = {
    "hello", "world", "from", "thread", "and", "clog"
  }

  uv.fs_unlink("test.log")
  local logger = log.init(2, "test.log")
  local work = uv.new_work(work_callback, after_work_callback)
  for i = 1, #items do
    work:queue(items[i])
  end
  local timer = uv.new_timer()
  timer:start(1000, 0, function()
    timer:close()
    local f = assert(io.open("test.log", "r"))
    local anothers = {}
    for line in f:lines() do
      anothers[#anothers + 1] = line
    end
    f:close()
    print(inspect(results))
    print(inspect(anothers))
    print(inspect(items))
    logger:close()
  end)
  uv.run()
  print('test thread ok')
end
