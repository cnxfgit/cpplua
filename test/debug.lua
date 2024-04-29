-- 定义钩子函数
function lineHook(event, line)
    print("Event:", event, "Line:", line)
end

-- 注册钩子函数，捕获行事件
debug.sethook(lineHook, "l")

-- 执行一段 Lua 代码
for i = 1, 5 do
    print("Line "..i)
end

-- 取消钩子
debug.sethook(nil)
-- 执行一段 Lua 代码
for i = 1, 5 do
    print("Line "..i)
end