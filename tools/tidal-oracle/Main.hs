-- Prints the events real Tidal produces for each conformance case, in the
-- format tests/test_conformance.cpp reads. Run through generate.py, which
-- writes Cases.hs first.
module Main (main) where

import Data.List (sort)
import Sound.Tidal.Context

import Cases (cases)

showTime :: Time -> String
showTime t
  | denominator t == 1 = show (numerator t)
  | otherwise = show (numerator t) ++ "/" ++ show (denominator t)

showEvent :: Event String -> String
showEvent e = unwords [wholeBegin, wholeEnd, showTime (start (part e)), showTime (stop (part e)), value e]
  where
    (wholeBegin, wholeEnd) = maybe ("-", "-") (\w -> (showTime (start w), showTime (stop w))) (whole e)

main :: IO ()
main = do
  putStrLn ("# tidal " ++ tidal_version)
  mapM_ printCase cases
  where
    printCase (name, cycleCount, pat) = do
      putStrLn ("case " ++ name)
      mapM_ putStrLn (sort (map showEvent (queryArc pat (Arc 0 (fromIntegral cycleCount)))))
      putStrLn "end"
