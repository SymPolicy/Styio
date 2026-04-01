### Verbatim String Literal
```
@("\ |String| /")
```

### Date
```
@("1998/12/01": date)
@("1998-12-01": date)
```

#### Date Operations
```
@(1998: date.year) & @(12: date.month) & @(1: date.day)
```

```
@("1998-12-01": date) - @(90: date.day)
```